from typing import Any, List
from fastapi import APIRouter, Depends, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse
from sqlalchemy.orm import Session
import os
import shutil
import tempfile

from app.db.session import get_db
from app import schemas, models
from app.core.config import settings

router = APIRouter()

@router.get("/{record_id}", response_model=schemas.AudioRecord, summary="Get a specific record")
def read_record(
    record_id: int,
    db: Session = Depends(get_db),
) -> Any:
    """
    Retrieve details of a specific audio record by its ID.
    """
    db_obj = db.query(models.AudioRecord).filter(models.AudioRecord.id == record_id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Record not found")
    
    if db_obj.file_path:
        db_obj.download_url = f"{settings.API_V1_STR}/audio_records/{db_obj.id}/download"
        
    return db_obj

@router.delete("/{record_id}", response_model=schemas.AudioRecord, summary="Delete a specific record")
def delete_record(
    record_id: int,
    db: Session = Depends(get_db),
) -> Any:
    """
    Delete a specific audio record by its ID and remove the associated file.
    """
    db_obj = db.query(models.AudioRecord).filter(models.AudioRecord.id == record_id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Record not found")
    
    # Remove file if it exists
    if os.path.exists(db_obj.file_path):
        os.remove(db_obj.file_path)
    
    db.delete(db_obj)
    db.commit()
    return db_obj

def list_device_records(
    device_id: str,
    db: Session,
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Internal helper to list records for a device.
    """
    records = db.query(models.AudioRecord).filter(models.AudioRecord.device_id == device_id).offset(skip).limit(limit).all()
    for record in records:
        if record.file_path:
            # Use the download endpoint instead of direct static file access 
            # to ensure we serve a stable copy and avoid "Content-Length" errors.
            record.download_url = f"{settings.API_V1_STR}/audio_records/{record.id}/download"
    return records

def delete_device_records(
    device_id: str,
    db: Session,
) -> Any:
    """
    Internal helper to delete all records for a device.
    """
    records = db.query(models.AudioRecord).filter(models.AudioRecord.device_id == device_id).all()
    
    for record in records:
        if os.path.exists(record.file_path):
            os.remove(record.file_path)
        db.delete(record)
    
    db.commit()
    return {"status": "ok", "deleted_count": len(records)}

@router.get("/{record_id}/download", summary="Download a specific record")
def download_record(
    record_id: int,
    background_tasks: BackgroundTasks,
    db: Session = Depends(get_db),
) -> Any:
    """
    Download the WAV file of a specific record.
    """
    db_obj = db.query(models.AudioRecord).filter(models.AudioRecord.id == record_id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Record not found")
    
    if not os.path.exists(db_obj.file_path):
        raise HTTPException(status_code=404, detail="File not found on disk")
    
    filename = os.path.basename(db_obj.file_path)
    
    # Create a temporary file to serve a "snapshot" of the file as it exists now.
    # This prevents "RuntimeError: Response content longer than Content-Length" 
    # if the file is still being written to.
    fd, temp_path = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    shutil.copy2(db_obj.file_path, temp_path)
    
    # Add background task to remove the temporary file after the response is sent.
    background_tasks.add_task(os.remove, temp_path)
    
    return FileResponse(path=temp_path, filename=filename, media_type='audio/wav')
