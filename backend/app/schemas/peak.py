from pydantic import BaseModel


class Peak(BaseModel):
    id: int
    device_id: str
    peak_time_ns: int
    received_at_ns: int
    processed: bool

    class Config:
        from_attributes = True
