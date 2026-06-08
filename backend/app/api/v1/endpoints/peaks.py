from typing import Any, List

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from app import models, schemas
from app.db.session import get_db

router = APIRouter()


@router.get("/", response_model=List[schemas.Peak], summary="List all peaks")
def list_peaks(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 1000,
) -> Any:
    return (
        db.query(models.Peak)
        .order_by(models.Peak.peak_time_ns.desc())
        .offset(skip)
        .limit(limit)
        .all()
    )


@router.delete("/{peak_id}", response_model=schemas.Peak, summary="Delete a peak")
def delete_peak(
    *,
    db: Session = Depends(get_db),
    peak_id: int,
) -> Any:
    peak = db.query(models.Peak).filter(models.Peak.id == peak_id).first()
    if not peak:
        raise HTTPException(status_code=404, detail="Peak not found")
    db.delete(peak)
    db.commit()
    return peak


@router.delete("/", summary="Delete all peaks")
def clear_peaks(db: Session = Depends(get_db)) -> Any:
    db.query(models.Peak).delete()
    db.commit()
    return {"status": "success", "message": "All peaks deleted"}
