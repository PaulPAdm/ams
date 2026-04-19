from pydantic import BaseModel
from datetime import datetime
from typing import Optional

class PeakBase(BaseModel):
    peak_time: datetime

class PeakCreate(PeakBase):
    pass

class PeakInDBBase(PeakBase):
    id: int
    device_id: str
    received_at: datetime

    class Config:
        from_attributes = True

class Peak(PeakInDBBase):
    pass
