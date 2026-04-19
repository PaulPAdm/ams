from typing import Any, List
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from app.db.session import get_db
from app import schemas, models
from app.api.v1.endpoints import locations, audio_records, peaks

router = APIRouter()

# --- Device Management (from locations.py) ---

@router.post("/", response_model=schemas.Location, summary="Register device")
def create_device(
    *,
    db: Session = Depends(get_db),
    location_in: schemas.LocationCreate,
) -> Any:
    """
    Registers a new device (microphone).
    """
    return locations.create_location(db=db, location_in=location_in)


@router.get("/{device_id}", response_model=schemas.Location, summary="Get device by ID")
def read_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Retrieve details of a specific device.
    """
    return locations.read_location(db=db, id=device_id)


@router.get("/", response_model=List[schemas.Location], summary="List all devices")
def list_devices(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve a list of all registered devices with their coordinates.
    """
    return locations.read_locations(db=db, skip=skip, limit=limit)

@router.put("/{device_id}", response_model=schemas.Location, summary="Update device")
def update_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    location_in: schemas.LocationUpdate,
) -> Any:
    """
    Update device settings (name, tag, location).
    """
    return locations.update_location(db=db, id=device_id, location_in=location_in)

@router.delete("/{device_id}", response_model=schemas.Location, summary="Delete device")
def delete_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Delete a device by its ID.
    """
    return locations.delete_location(db=db, id=device_id)

@router.post("/{device_id}/init", response_model=schemas.DeviceInitResponse, summary="Initialize device and sync time")
def init_device(
    *,
    db: Session = Depends(get_db),
    device_id: str
) -> Any:
    """
    Initializes a device and returns the current server time for synchronization.
    """
    return locations.init_device(db=db, id=device_id)

# --- Device Audio Records (from audio_records.py) ---

@router.get("/{device_id}/audio_records/{record_id}", response_model=schemas.AudioRecord, summary="Get a specific record for a device")
def read_device_audio_record(
    device_id: str,
    record_id: int,
    db: Session = Depends(get_db),
) -> Any:
    """
    Retrieve details of a specific audio record for a specific device.
    """
    record = audio_records.read_record(record_id=record_id, db=db)
    if record.device_id != device_id:
        raise HTTPException(status_code=404, detail="Record not found for this device")
    return record

@router.get("/{device_id}/audio_records", response_model=List[schemas.AudioRecord], summary="List all records for a device")
def list_device_audio_records(
    device_id: str,
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve a list of all audio records for a specific device.
    """
    return audio_records.list_device_records(device_id=device_id, db=db, skip=skip, limit=limit)

@router.delete("/{device_id}/audio_records", summary="Delete all records for a device")
def delete_device_audio_records(
    device_id: str,
    db: Session = Depends(get_db),
) -> Any:
    """
    Delete all audio records for a specific device and remove their associated files.
    """
    return audio_records.delete_device_records(device_id=device_id, db=db)

# --- Device Peaks (from peaks.py) ---

@router.get("/{device_id}/peaks", response_model=List[schemas.Peak], summary="Get peaks by device")
def read_device_peaks(
    device_id: str,
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve all peaks registered by a specific device.
    """
    return peaks.read_peaks_by_device(device_id=device_id, db=db, skip=skip, limit=limit)
