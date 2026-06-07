from sqlalchemy import BigInteger, Boolean, Column, ForeignKey, Index, Integer, String
from app.db.base_class import Base
from app.core.time_utils import epoch_ns

class Peak(Base):
    __table_args__ = (
        Index("ix_peak_device_id_peak_time_ns", "device_id", "peak_time_ns"),
        Index("ix_peak_processed_peak_time_ns", "processed", "peak_time_ns"),
    )

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True)
    peak_time_ns = Column(BigInteger, index=True, nullable=False)
    received_at_ns = Column(BigInteger, default=epoch_ns, index=True, nullable=False)
    processed = Column(Boolean, default=False, nullable=False, index=True)
