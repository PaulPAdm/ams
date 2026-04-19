from sqlalchemy import Column, String
from geoalchemy2 import Geometry
from app.db.base_class import Base

class Location(Base):
    id = Column(String, primary_key=True, index=True)
    name = Column(String, index=True)
    tag = Column(String, index=True)
    geom = Column(Geometry(geometry_type='POINT', srid=4326))
