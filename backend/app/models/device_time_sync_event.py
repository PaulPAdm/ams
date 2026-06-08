from sqlalchemy import BigInteger, Column, ForeignKey, Index, Integer, String

from app.core.time_utils import epoch_ns
from app.db.base_class import Base


class DeviceTimeSyncEvent(Base):
    __tablename__ = "device_time_sync_events"
    __table_args__ = (
        Index("ix_device_time_sync_events_device_id_created_at_ns", "device_id", "created_at_ns"),
    )

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True, nullable=False)
    client_sent_monotonic_ns = Column(BigInteger, nullable=False)
    server_received_epoch_ns = Column(BigInteger, nullable=False)
    server_transmit_epoch_ns = Column(BigInteger, nullable=False)
    created_at_ns = Column(BigInteger, default=epoch_ns, index=True, nullable=False)
