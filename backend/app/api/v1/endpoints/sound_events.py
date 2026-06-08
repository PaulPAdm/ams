import logging
import os
import struct
from typing import Any, List

from fastapi import APIRouter, Depends, HTTPException, Request
from fastapi.responses import FileResponse, Response
from sqlalchemy.orm import Session

from app import models, schemas
from app.core.config import settings
from app.db.session import get_db
from app.services.triangulation import process_recent_peaks

router = APIRouter()
logger = logging.getLogger(__name__)

_CONTENT_TYPE_TO_EXT: dict[str, str] = {
    "audio/wav": ".wav",
    "audio/wave": ".wav",
    "audio/x-wav": ".wav",
    "audio/mpeg": ".mp3",
    "audio/mp3": ".mp3",
    "audio/ogg": ".ogg",
    "audio/flac": ".flac",
    "audio/mp4": ".mp4",
    "audio/aac": ".aac",
    "audio/webm": ".webm",
    "application/octet-stream": ".pcm16",
}


def _ext_for_content_type(content_type: str) -> str:
    base = content_type.split(";")[0].strip().lower()
    return _CONTENT_TYPE_TO_EXT.get(base, ".pcm16")


# Raw PCM the device uploads has no container, so browsers cannot play it via
# <audio>. These are the content types / extensions we wrap into WAV on the fly.
_RAW_PCM_CONTENT_TYPES = {
    "application/vnd.ams.pcm16",
    "application/octet-stream",
    "audio/l16",
}
_RAW_PCM_EXTENSIONS = (".pcm16", ".pcm", ".raw")


def _is_raw_pcm(content_type: str | None, file_path: str) -> bool:
    base = (content_type or "").split(";")[0].strip().lower()
    if base in _RAW_PCM_CONTENT_TYPES:
        return True
    return file_path.lower().endswith(_RAW_PCM_EXTENSIONS)


def _wav_header(data_len: int, sample_rate: int, channels: int, bits_per_sample: int = 16) -> bytes:
    byte_rate = sample_rate * channels * bits_per_sample // 8
    block_align = channels * bits_per_sample // 8
    return struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF",
        36 + data_len,
        b"WAVE",
        b"fmt ",
        16,            # PCM fmt chunk size
        1,             # audio format = PCM
        channels,
        sample_rate,
        byte_rate,
        block_align,
        bits_per_sample,
        b"data",
        data_len,
    )


def _require_device(db: Session, device_id: str) -> models.Location:
    device = db.query(models.Location).filter(models.Location.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


def _with_audio_url(event: models.SoundEvent) -> schemas.SoundEvent:
    dto = schemas.SoundEvent.model_validate(event)
    # Serve through the API endpoint, which wraps raw PCM into WAV so the
    # browser <audio> element can play it. Falls back to None when no audio.
    if event.audio_uploaded and event.audio_file_path:
        dto.audio_download_url = f"/api/devices/{event.device_id}/sound-events/{event.id}/audio"
    else:
        dto.audio_download_url = None
    return dto


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
    Upload the audio window for a detected sound event.

    Accepts either:
    - Raw bytes with Content-Type: audio/wav, audio/mpeg, etc. (embedded device)
    - multipart/form-data with a 'file' field (browser upload)
    """
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event:
        raise HTTPException(status_code=404, detail="Sound event not found")

    content_type_header = request.headers.get("content-type", "application/octet-stream")

    if "multipart/form-data" in content_type_header:
        form = await request.form()
        upload_file = form.get("file")
        if not upload_file:
            raise HTTPException(status_code=400, detail="No 'file' field in multipart form data")
        payload = await upload_file.read()
        actual_content_type = upload_file.content_type or "application/octet-stream"
    else:
        payload = await request.body()
        actual_content_type = content_type_header

    if not payload:
        raise HTTPException(status_code=400, detail="Audio payload is empty")

    extension = _ext_for_content_type(actual_content_type)
    events_dir = os.path.join(settings.AUDIO_STORAGE_PATH, "events")
    os.makedirs(events_dir, exist_ok=True)

    file_name = f"{device_id}_{event_id}{extension}"
    file_path = os.path.join(events_dir, file_name)
    with open(file_path, "wb") as audio_file:
        audio_file.write(payload)

    event.audio_file_path = file_path
    event.audio_content_type = actual_content_type
    event.audio_size_bytes = len(payload)
    event.audio_uploaded = True
    db.commit()
    db.refresh(event)

    logger.info("Stored audio for sound event %s from %s: %s bytes (%s)", event_id, device_id, len(payload), actual_content_type)
    return _with_audio_url(event)


@router.get("/{device_id}/sound-events/{event_id}/audio", summary="Download event audio")
def download_device_sound_event_audio(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    event_id: int,
) -> Any:
    """
    Stream the stored audio for a sound event.

    Raw PCM16 (what the embedded device uploads) is wrapped in a WAV container
    on the fly using the event's sample_rate_hz/channels so it plays in the
    browser. Already-containerized formats (wav/mp3/...) are served as-is.
    """
    event = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.id == event_id, models.SoundEvent.device_id == device_id)
        .first()
    )
    if not event or not event.audio_file_path or not os.path.isfile(event.audio_file_path):
        raise HTTPException(status_code=404, detail="Audio not found for sound event")

    if _is_raw_pcm(event.audio_content_type, event.audio_file_path):
        with open(event.audio_file_path, "rb") as audio_file:
            pcm = audio_file.read()
        body = _wav_header(len(pcm), event.sample_rate_hz, event.channels) + pcm
        return Response(
            content=body,
            media_type="audio/wav",
            headers={"Content-Disposition": f'inline; filename="sound-event-{event_id}.wav"'},
        )

    return FileResponse(
        event.audio_file_path,
        media_type=event.audio_content_type or "application/octet-stream",
        filename=f"sound-event-{event_id}{os.path.splitext(event.audio_file_path)[1]}",
    )


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


@router.delete("/{device_id}/sound-events/{event_id}", response_model=schemas.SoundEvent, summary="Delete a sound event")
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

    result = _with_audio_url(event)

    if event.audio_file_path and os.path.isfile(event.audio_file_path):
        try:
            os.remove(event.audio_file_path)
        except OSError:
            logger.warning("Could not delete audio file %s for event %s", event.audio_file_path, event_id)

    db.delete(event)
    db.commit()
    return result


@router.delete("/{device_id}/sound-events", summary="Delete all sound events for a device")
def clear_device_sound_events(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    _require_device(db, device_id)

    events = (
        db.query(models.SoundEvent)
        .filter(models.SoundEvent.device_id == device_id)
        .all()
    )
    for event in events:
        if event.audio_file_path and os.path.isfile(event.audio_file_path):
            try:
                os.remove(event.audio_file_path)
            except OSError:
                logger.warning("Could not delete audio file %s", event.audio_file_path)

    db.query(models.SoundEvent).filter(models.SoundEvent.device_id == device_id).delete(synchronize_session=False)
    db.commit()
    return {"status": "success", "message": f"All sound events for device {device_id} deleted"}
