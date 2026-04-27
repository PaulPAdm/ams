from sqlalchemy import Column, String, DateTime, ForeignKey, Integer, Boolean
from app.db.base_class import Base
import datetime

class Peak(Base):
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True)
    peak_time = Column(DateTime(timezone=True), index=True) # Exact peak time
    received_at = Column(DateTime(timezone=True), default=lambda: datetime.datetime.now())
    processed = Column(Boolean, default=False, nullable=False, index=True)
