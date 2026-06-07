from typing import Optional

from pydantic import BaseModel, Field


class SoundEventBase(BaseModel):
    event_time_ns: int = Field(..., gt=0, description="Exact UTC time of the detected acoustic peak, in nanoseconds.")
    duration_ms: int = Field(default=10000, ge=0)
    pre_event_ms: int = Field(default=5000, ge=0)
    post_event_ms: int = Field(default=5000, ge=0)
    sample_rate_hz: int = Field(default=16000, ge=1, le=384000)
    channels: int = Field(default=1, ge=1, le=8)
    sample_format: str = "pcm16le"
    peak_level: Optional[float] = None
    rms_level: Optional[float] = None
    noise_floor: Optional[float] = None
    detector_version: str = "embedded-basic-v1"
    detection_label: str = "impulse"


class SoundEventCreate(SoundEventBase):
    pass


class SoundEvent(SoundEventBase):
    id: int
    device_id: str
    peak_id: Optional[int] = None
    received_at_ns: int
    audio_file_path: Optional[str] = None
    audio_content_type: Optional[str] = None
    audio_size_bytes: Optional[int] = None
    audio_uploaded: bool
    processed: bool
    audio_download_url: Optional[str] = None

    class Config:
        from_attributes = True
