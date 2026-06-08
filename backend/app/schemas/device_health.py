from typing import Any, Optional

from pydantic import BaseModel, Field, field_validator


UNKNOWN_VALUES = {"", "unknown", "unknow", "n/a", "na", "none", "null"}


def _coerce_unknown(value: Any) -> Any:
    if isinstance(value, str) and value.strip().lower() in UNKNOWN_VALUES:
        return None
    return value


class DeviceHealthReportBase(BaseModel):
    firmware_version: Optional[str] = Field(default=None, max_length=64)
    status_message: Optional[str] = Field(default=None, max_length=255)
    uptime_ms: Optional[int] = Field(default=None, ge=0)
    wifi_connected: Optional[bool] = None
    microphone_active: Optional[bool] = None
    ina219_online: Optional[bool] = None
    bus_voltage_v: Optional[float] = None
    shunt_voltage_mv: Optional[float] = None
    current_ma: Optional[float] = None
    power_mw: Optional[float] = None
    computed_power_mw: Optional[float] = None
    audio_queue_depth: Optional[int] = Field(default=None, ge=0)
    audio_dropped_chunks: Optional[int] = Field(default=None, ge=0)

    @field_validator(
        "ina219_online",
        "bus_voltage_v",
        "shunt_voltage_mv",
        "current_ma",
        "power_mw",
        "computed_power_mw",
        mode="before",
    )
    @classmethod
    def coerce_unknown_power_fields(cls, value: Any) -> Any:
        return _coerce_unknown(value)


class DeviceHealthReportCreate(DeviceHealthReportBase):
    # Optional capture time (device UTC ns). When the device buffers reports while
    # offline and flushes them later, this preserves the original timeline instead
    # of using the server receive time. Stored as received_at_ns when provided.
    captured_at_ns: Optional[int] = Field(default=None, ge=0)

    @field_validator("captured_at_ns", mode="before")
    @classmethod
    def coerce_unknown_captured_at(cls, value: Any) -> Any:
        return _coerce_unknown(value)


class DeviceHealthReport(DeviceHealthReportBase):
    id: int
    device_id: str
    received_at_ns: int

    class Config:
        from_attributes = True
