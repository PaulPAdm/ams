from typing import Any, List
from fastapi import APIRouter, Depends, HTTPException, Header
from sqlalchemy.orm import Session
from app.db.session import get_db
from app import schemas, models
from app.services.triangulation import process_recent_peaks

router = APIRouter()

@router.post("/", response_model=schemas.Peak, summary="Register sound peak")
def create_peak(
    *,
    db: Session = Depends(get_db),
    peak_in: schemas.PeakCreate,
    x_device_id: str = Header(..., alias="X-Device-ID", description="ID of the device that captured the peak")
) -> Any:
    """
    Creates a new sound peak record.
    After saving, the triangulation process is automatically triggered to locate the sound source.
    """
    # Check if the device exists
    device = db.query(models.Location).filter(models.Location.id == x_device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    
    db_obj = models.Peak(
        device_id=x_device_id,
        peak_time=peak_in.peak_time
    )
    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)
    
    # Run triangulation immediately after saving the peak.
    process_recent_peaks(db)
    
    return db_obj

@router.get("/", response_model=List[schemas.Peak], summary="Get all peaks")
def read_peaks(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve a list of all registered sound peaks.
    """
    peaks = db.query(models.Peak).offset(skip).limit(limit).all()
    return peaks


def read_peaks_by_device(
    device_id: str,
    db: Session,
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Internal helper to retrieve all peaks registered by a specific device.
    """
    peaks = db.query(models.Peak).filter(models.Peak.device_id == device_id).offset(skip).limit(limit).all()
    return peaks


@router.delete("/{peak_id}", response_model=schemas.Peak, summary="Delete sound peak")
def delete_peak(
    *,
    db: Session = Depends(get_db),
    peak_id: int
) -> Any:
    """
    Deletes a sound peak record by ID.
    """
    peak = db.query(models.Peak).filter(models.Peak.id == peak_id).first()
    if not peak:
        raise HTTPException(status_code=404, detail="Peak not found")
    
    db.delete(peak)
    db.commit()
    return peak


@router.delete("/", summary="Clear all sound peaks")
def clear_peaks(
    db: Session = Depends(get_db)
) -> Any:
    """
    Deletes all registered sound peaks.
    """
    db.query(models.Peak).delete()
    db.commit()
    return {"status": "success", "message": "All peaks deleted"}
