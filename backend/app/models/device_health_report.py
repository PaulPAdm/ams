from sqlalchemy import BigInteger, Boolean, Column, Float, ForeignKey, Index, Integer, String

from app.core.time_utils import epoch_ns
from app.db.base_class import Base


class DeviceHealthReport(Base):
    __tablename__ = "device_health_reports"
    __table_args__ = (
        Index("ix_device_health_reports_device_id_received_at_ns", "device_id", "received_at_ns"),
    )

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True, nullable=False)
    firmware_version = Column(String, nullable=True)
    status_message = Column(String, nullable=True)
    uptime_ms = Column(Integer, nullable=True)
    wifi_connected = Column(Boolean, nullable=True)
    microphone_active = Column(Boolean, nullable=True)
    ina219_online = Column(Boolean, nullable=True)
    bus_voltage_v = Column(Float, nullable=True)
    shunt_voltage_mv = Column(Float, nullable=True)
    current_ma = Column(Float, nullable=True)
    power_mw = Column(Float, nullable=True)
    computed_power_mw = Column(Float, nullable=True)
    audio_queue_depth = Column(Integer, nullable=True)
    audio_dropped_chunks = Column(Integer, nullable=True)
    received_at_ns = Column(BigInteger, default=epoch_ns, index=True, nullable=False)
