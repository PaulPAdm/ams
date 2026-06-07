import logging
import os
import wave
from io import BytesIO
from typing import Any, List

import anyio
from fastapi import APIRouter, Depends, HTTPException, Request
from fastapi.responses import FileResponse
from sqlalchemy.orm import Session

from app import models, schemas
from app.core.config import settings
from app.db.session import get_db
from app.services.triangulation import process_recent_peaks

router = APIRouter()
logger = logging.getLogger(__name__)

RAW_PCM_CONTENT_TYPES = {
    "application/vnd.ams.pcm16",
    "audio/pcm",
    "audio/l16",
}

AUDIO_EXTENSION_BY_CONTENT_TYPE = {
    "audio/wav": ".wav",
    "audio/wave": ".wav",
    "audio/x-wav": ".wav",
    "audio/vnd.wave": ".wav",
    "audio/mpeg": ".mp3",
    "audio/mp3": ".mp3",
    "audio/ogg": ".ogg",
    "audio/flac": ".flac",
    "audio/x-flac": ".flac",
    "audio/mp4": ".m4a",
    "audio/x-m4a": ".m4a",
    "audio/aac": ".aac",
    "audio/webm": ".webm",
}

AUDIO_CONTENT_TYPE_BY_EXTENSION = {
    ".wav": "audio/wav",
    ".mp3": "audio/mpeg",
    ".ogg": "audio/ogg",
    ".oga": "audio/ogg",
    ".flac": "audio/flac",
    ".m4a": "audio/mp4",
    ".mp4": "audio/mp4",
    ".aac": "audio/aac",
    ".webm": "audio/webm",
}

ALLOWED_AUDIO_EXTENSIONS = {
    ".wav",
    ".mp3",
    ".ogg",
    ".oga",
    ".flac",
    ".m4a",
    ".mp4",
    ".aac",
    ".webm",
}


def _require_device(db: Session, device_id: str) -> models.Location:
    device = db.query(models.Location).filter(models.Location.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


def _stored_audio_path(file_path: str | None) -> str | None:
    if not file_path:
        return None

    storage_root = os.path.abspath(settings.AUDIO_STORAGE_PATH)
    absolute_path = os.path.abspath(file_path)
    try:
        relative_path = os.path.relpath(absolute_path, storage_root)
    except ValueError:
        return None

    if relative_path == os.pardir or relative_path.startswith(os.pardir + os.sep):
        return None

    return absolute_path if os.path.isfile(absolute_path) else None


def _audio_download_url(event: models.SoundEvent) -> str | None:
    if not event.audio_file_path:
        return None

    return f"/api/devices/{event.device_id}/sound-events/{event.id}/audio/download"


def _remove_stored_audio_file(file_path: str | None) -> None:
    absolute_path = _stored_audio_path(file_path)
    if absolute_path is None:
        return

    try:
        os.remove(absolute_path)
    except OSError:
        logger.warning("Failed to remove audio file %s", absolute_path, exc_info=True)


def _with_audio_url(event: models.SoundEvent) -> schemas.SoundEvent:
    dto = schemas.SoundEvent.model_validate(event)
    dto.audio_download_url = _audio_download_url(event)
    return dto


def _normalized_content_type(content_type: str | None) -> str:
    if not content_type:
        return "application/octet-stream"

    return content_type.split(";", 1)[0].strip().lower() or "application/octet-stream"


def _safe_file_stem(value: str) -> str:
    cleaned = "".join(char if char.isalnum() or char in {"-", "_"} else "_" for char in value)
    return cleaned.strip("_") or "audio"


def _extension_from_filename(filename: str | None) -> str | None:
    if not filename:
        return None

    extension = os.path.splitext(filename)[1].lower()
    return extension if extension in ALLOWED_AUDIO_EXTENSIONS else None


def _extension_for_audio(content_type: str, filename: str | None) -> str | None:
    return AUDIO_EXTENSION_BY_CONTENT_TYPE.get(content_type) or _extension_from_filename(filename)


def _pcm16le_to_wav(payload: bytes, event: models.SoundEvent) -> bytes:
    sample_format = (event.sample_format or "").lower()
    if sample_format != "pcm16le":
        raise HTTPException(status_code=415, detail=f"Raw audio sample format is not supported: {event.sample_format}")

    sample_rate_hz = event.sample_rate_hz or settings.AUDIO_SAMPLE_RATE
    channels = event.channels or settings.AUDIO_CHANNELS
    if channels <= 0 or sample_rate_hz <= 0:
        raise HTTPException(status_code=400, detail="Sound event audio metadata is invalid")

    output = BytesIO()
    with wave.open(output, "wb") as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate_hz)
        wav_file.writeframes(payload)

    return output.getvalue()


def _write_audio_file(file_path: str, payload: bytes) -> None:
    with open(file_path, "wb") as f:
        f.write(payload)


def _store_event_audio(
    event: models.SoundEvent,
    device_id: str,
    payload: bytes,
    content_type: str,
    filename: str | None = None,
) -> None:
    if not payload:
        raise HTTPException(status_code=400, detail="Audio payload is empty")

    normalized_content_type = _normalized_content_type(content_type)
    filename_extension = os.path.splitext(filename or "")[1].lower()
    is_raw_pcm = (
        normalized_content_type in RAW_PCM_CONTENT_TYPES
        or filename_extension == ".pcm16"
        or (normalized_content_type == "application/octet-stream" and filename_extension == "")
    )

    if is_raw_pcm:
        stored_payload = _pcm16le_to_wav(payload, event)
        stored_content_type = "audio/wav"
        extension = ".wav"
    else:
        extension = _extension_for_audio(normalized_content_type, filename)
        if extension is None:
            raise HTTPException(status_code=415, detail=f"Unsupported audio format: {content_type}")

        stored_payload = payload
        stored_content_type = (
            AUDIO_CONTENT_TYPE_BY_EXTENSION.get(extension, normalized_content_type)
            if normalized_content_type == "application/octet-stream"
            else normalized_content_type
        )

    events_dir = os.path.join(settings.AUDIO_STORAGE_PATH, "events")
    os.makedirs(events_dir, exist_ok=True)

    file_name = f"{_safe_file_stem(device_id)}_{event.id}{extension}"
    file_path = os.path.join(events_dir, file_name)

    event.audio_file_path = file_path
    event.audio_content_type = stored_content_type
    event.audio_size_bytes = len(stored_payload)
    event.audio_uploaded = True

    return file_path, stored_payload


async def _read_audio_upload(request: Request) -> tuple[bytes, str, str | None]:
    content_type = request.headers.get("content-type", "application/octet-stream")
    if "multipart/form-data" not in content_type.lower():
        return await request.body(), content_type, None

    form = await request.form()
    upload = form.get("file")
    if upload is None or not hasattr(upload, "read"):
        raise HTTPException(status_code=400, detail="Multipart request must include a file field")

    payload = await upload.read()
    upload_content_type = getattr(upload, "content_type", None) or "application/octet-stream"
    filename = getattr(upload, "filename", None)
    return payload, upload_content_type, filename


@router.post("/{device_id}/sound-events", response_model=schemas.SoundEvent, summary="Register detected sound event")
def create_device_sound_event(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_in: schemas.SoundEventCreate,
) -> Any:
    """
    Accept metadata for a locally detected impulse sound.

    This is the new event-first flow: the device sends only a detected event,
    not a continuous audio stream. A Peak is created alongside the event so the
    existing TDOA triangulation pipeline can group events from multiple devices.
    """
    _require_device(db, device_id)

    peak = models.Peak(device_id=device_id, peak_time_ns=event_in.event_time_ns)
    db.add(peak)
    db.flush()

    event = models.SoundEvent(
        device_id=device_id,
        peak_id=peak.id,
        **event_in.model_dump(),
    )
    db.add(event)
    db.commit()
    db.refresh(event)

    try:
        process_recent_peaks(db)
    except Exception:
        logger.exception("Triangulation failed after sound event %s from %s", event.id, device_id)

    logger.info(
        "Sound event %s from %s: event_time_ns=%s peak=%s rms=%s audio_uploaded=%s",
        event.id,
        device_id,
        event.event_time_ns,
        event.peak_level,
        event.rms_level,
        event.audio_uploaded,
    )
    return _with_audio_url(event)


@router.post("/{device_id}/sound-events/{event_id}/audio", response_model=schemas.SoundEvent, summary="Upload event audio")
async def upload_device_sound_event_audio(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_id: int,
    request: Request,
) -> Any:
    """
    Upload an audio file for a detected sound event.

    The endpoint accepts browser `multipart/form-data` with a `file` field.
    Device raw PCM16LE uploads are still accepted as a raw body and are stored
    as WAV files.
    """
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event:
        raise HTTPException(status_code=404, detail="Sound event not found")

    payload, content_type, filename = await _read_audio_upload(request)
    file_path, stored_payload = _store_event_audio(event, device_id, payload, content_type, filename)
    await anyio.to_thread.run_sync(lambda: _write_audio_file(file_path, stored_payload))
    db.commit()
    db.refresh(event)

    logger.info(
        "Stored audio for sound event %s from %s: %s bytes (%s)",
        event_id,
        device_id,
        event.audio_size_bytes,
        event.audio_content_type,
    )
    return _with_audio_url(event)


@router.get("/{device_id}/sound-events", response_model=List[schemas.SoundEvent], summary="List device sound events")
def list_device_sound_events(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    skip: int = 0,
    limit: int = 100,
) -> Any:
    _require_device(db, device_id)
    events = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.device_id == device_id)
        .order_by(models.SoundEvent.event_time_ns.desc())
        .offset(skip)
        .limit(limit)
        .all()
    )
    return [_with_audio_url(event) for event in events]


@router.delete("/{device_id}/sound-events", summary="Delete device sound events")
def delete_device_sound_events(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    _require_device(db, device_id)
    events = db.query(models.SoundEvent).filter(models.SoundEvent.device_id == device_id).all()
    file_paths = [event.audio_file_path for event in events]
    peak_ids = [event.peak_id for event in events if event.peak_id is not None]
    deleted_count = len(events)

    for event in events:
        db.delete(event)

    if peak_ids:
        db.query(models.Peak).filter(models.Peak.id.in_(peak_ids)).delete(synchronize_session=False)

    db.commit()
    for file_path in file_paths:
        _remove_stored_audio_file(file_path)

    return {"deleted": deleted_count}


@router.get("/{device_id}/sound-events/{event_id}", response_model=schemas.SoundEvent, summary="Get device sound event")
def read_device_sound_event(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_id: int,
) -> Any:
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event:
        raise HTTPException(status_code=404, detail="Sound event not found")

    return _with_audio_url(event)


@router.get("/{device_id}/sound-events/{event_id}/audio/download", summary="Download device sound event audio")
def download_device_sound_event_audio(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_id: int,
) -> FileResponse:
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event:
        raise HTTPException(status_code=404, detail="Sound event not found")

    audio_path = _stored_audio_path(event.audio_file_path)
    if audio_path is None:
        raise HTTPException(status_code=404, detail="Sound event audio not found")

    return FileResponse(
        audio_path,
        media_type=event.audio_content_type or "application/octet-stream",
        filename=os.path.basename(audio_path),
        content_disposition_type="inline",
    )


@router.delete("/{device_id}/sound-events/{event_id}", summary="Delete device sound event")
def delete_device_sound_event(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_id: int,
) -> Any:
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event:
        raise HTTPException(status_code=404, detail="Sound event not found")

    file_path = event.audio_file_path
    peak_id = event.peak_id
    db.delete(event)
    if peak_id is not None:
        peak = db.query(models.Peak).filter(models.Peak.id == peak_id).first()
        if peak is not None:
            db.delete(peak)

    db.commit()
    _remove_stored_audio_file(file_path)
    return {"deleted": 1}
