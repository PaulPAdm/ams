from pydantic import BaseModel
from pydantic import Field
from typing import Optional
from datetime import datetime

class LocationBase(BaseModel):
    name: str
    tag: Optional[str] = None

class LocationCreate(LocationBase):
    id: Optional[str] = Field(default=None, min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")
    lat: float
    lon: float

class LocationUpdate(BaseModel):
    name: Optional[str] = None
    tag: Optional[str] = None
    lat: Optional[float] = None
    lon: Optional[float] = None

class LocationInDBBase(LocationBase):
    id: str

    class Config:
        from_attributes = True

class Location(LocationInDBBase):
    lat: Optional[float] = None
    lon: Optional[float] = None

class DeviceInitResponse(BaseModel):
    device_id: str
    server_time: datetime
