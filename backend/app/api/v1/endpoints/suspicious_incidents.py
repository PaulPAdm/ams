from typing import Any, List
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from geoalchemy2.shape import from_shape, to_shape
from shapely.geometry import Point
from app.db.session import get_db
from app import schemas, models

router = APIRouter()

@router.post("/", response_model=schemas.SuspiciousIncident, summary="Create suspicious incident")
def create_suspicious_incident(
    *,
    db: Session = Depends(get_db),
    incident_in: schemas.SuspiciousIncidentCreate,
) -> Any:
    """
    Creates a suspicious incident record.
    Coordinates are optional and stored in PostGIS when both lat and lon are provided.
    """
    if (incident_in.lat is None) != (incident_in.lon is None):
        raise HTTPException(status_code=400, detail="Both lat and lon must be provided together")

    db_obj = models.SuspiciousIncident(
        description=incident_in.description,
        geom=from_shape(Point(incident_in.lon, incident_in.lat), srid=4326)
        if incident_in.lat is not None and incident_in.lon is not None
        else None
    )
    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)

    if db_obj.geom:
        point = to_shape(db_obj.geom)
        db_obj.lat = point.y
        db_obj.lon = point.x
    else:
        db_obj.lat = None
        db_obj.lon = None

    return db_obj

@router.get("/", response_model=List[schemas.SuspiciousIncident], summary="Get all suspicious incidents")
def read_suspicious_incidents(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Returns a list of all suspicious incidents with their description, coordinates and creation time.
    """
    incidents = db.query(models.SuspiciousIncident).order_by(models.SuspiciousIncident.created_at.desc()).offset(skip).limit(limit).all()
    for incident in incidents:
        if incident.geom:
            point = to_shape(incident.geom)
            incident.lat = point.y
            incident.lon = point.x
        else:
            incident.lat = None
            incident.lon = None
    return incidents

@router.get("/{id}", response_model=schemas.SuspiciousIncident, summary="Get suspicious incident by ID")
def read_suspicious_incident(
    *,
    db: Session = Depends(get_db),
    id: int,
) -> Any:
    """
    Returns a suspicious incident record by its ID.
    """
    incident = db.query(models.SuspiciousIncident).filter(models.SuspiciousIncident.id == id).first()
    if not incident:
        raise HTTPException(status_code=404, detail="Incident not found")
    
    if incident.geom:
        point = to_shape(incident.geom)
        incident.lat = point.y
        incident.lon = point.x
    else:
        incident.lat = None
        incident.lon = None
    return incident

@router.delete("/{id}", response_model=schemas.SuspiciousIncident, summary="Delete suspicious incident")
def delete_suspicious_incident(
    *,
    db: Session = Depends(get_db),
    id: int,
) -> Any:
    """
    Deletes a record of a suspicious incident by its ID.
    """
    db_obj = db.query(models.SuspiciousIncident).filter(models.SuspiciousIncident.id == id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Incident not found")
    
    # We need to manually populate lat/lon before deleting if we want it in the response model
    if db_obj.geom:
        point = to_shape(db_obj.geom)
        db_obj.lat = point.y
        db_obj.lon = point.x
    else:
        db_obj.lat = None
        db_obj.lon = None

    db.delete(db_obj)
    db.commit()
    return db_obj

@router.delete("/", summary="Clear all suspicious incidents")
def clear_suspicious_incidents(
    db: Session = Depends(get_db)
) -> Any:
    """
    Deletes all registered suspicious incidents.
    """
    db.query(models.SuspiciousIncident).delete()
    db.commit()
    return {"status": "success", "message": "All incidents deleted"}
