from sqlalchemy import Column, Integer, DateTime, String
from geoalchemy2 import Geometry
from app.db.base_class import Base
import datetime

class SuspiciousIncident(Base):
    __tablename__ = "suspicious_incidents"
    id = Column(Integer, primary_key=True, index=True)
    description = Column(String, nullable=True)
    geom = Column(Geometry(geometry_type='POINT', srid=4326))
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.datetime.now())
