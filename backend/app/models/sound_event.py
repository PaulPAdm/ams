from sqlalchemy import BigInteger, Boolean, Column, Float, ForeignKey, Index, Integer, String
from sqlalchemy.orm import relationship

from app.core.time_utils import epoch_ns
from app.db.base_class import Base


class SoundEvent(Base):
    __tablename__ = "sound_events"
    __table_args__ = (
        Index("ix_sound_events_device_id_event_time_ns", "device_id", "event_time_ns"),
    )

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("location.id", ondelete="CASCADE"), index=True, nullable=False)
    peak_id = Column(Integer, ForeignKey("peak.id", ondelete="SET NULL"), nullable=True)

    event_time_ns = Column(BigInteger, index=True, nullable=False)
    received_at_ns = Column(BigInteger, default=epoch_ns, index=True, nullable=False)

    duration_ms = Column(Integer, default=10000, nullable=False)
    pre_event_ms = Column(Integer, default=5000, nullable=False)
    post_event_ms = Column(Integer, default=5000, nullable=False)
    sample_rate_hz = Column(Integer, default=16000, nullable=False)
    channels = Column(Integer, default=1, nullable=False)
    sample_format = Column(String, default="pcm16le", nullable=False)

    peak_level = Column(Float, nullable=True)
    rms_level = Column(Float, nullable=True)
    noise_floor = Column(Float, nullable=True)
    detector_version = Column(String, default="embedded-basic-v1", nullable=False)
    detection_label = Column(String, default="impulse", nullable=False)

    audio_file_path = Column(String, nullable=True)
    audio_content_type = Column(String, nullable=True)
    audio_size_bytes = Column(Integer, nullable=True)
    audio_uploaded = Column(Boolean, default=False, nullable=False, index=True)
    processed = Column(Boolean, default=False, nullable=False, index=True)

    device = relationship("Location", backref="sound_events")
    peak = relationship("Peak")
