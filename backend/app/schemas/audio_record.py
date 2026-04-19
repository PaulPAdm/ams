from typing import Optional
from datetime import datetime
from pydantic import BaseModel

class AudioRecordBase(BaseModel):
    device_id: str
    file_path: str

class AudioRecordCreate(AudioRecordBase):
    pass

class AudioRecord(AudioRecordBase):
    id: int
    created_at: datetime
    download_url: Optional[str] = None

    class Config:
        from_attributes = True
