from sqlalchemy import Column, String, DateTime, ForeignKey, Integer
from sqlalchemy.orm import relationship
from datetime import datetime, timezone
from app.db.base_class import Base

class AudioRecord(Base):
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True)
    file_path = Column(String, nullable=False)
    created_at = Column(DateTime, default=lambda: datetime.now())
    
    device = relationship("Location", backref="audio_records")
