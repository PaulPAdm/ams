from typing import Any, List
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from geoalchemy2.shape import to_shape, from_shape
from shapely.geometry import Point
import uuid
from datetime import datetime
from app.db.session import get_db
from app import schemas, models

router = APIRouter()

@router.post("/", response_model=schemas.Location, summary="Register device")
def create_location(
    *,
    db: Session = Depends(get_db),
    location_in: schemas.LocationCreate,
) -> Any:
    """
    Registers a new device (microphone).
    If ID is not provided, it will be automatically generated (UUID).
    Coordinates (lat, lon) are stored in PostGIS.
    """
    device_id = location_in.id or str(uuid.uuid4())
    
    # Check if already exists
    db_obj = db.query(models.Location).filter(models.Location.id == device_id).first()
    if db_obj:
        raise HTTPException(
            status_code=400,
            detail=f"Device with ID {device_id} already exists."
        )

    db_obj = models.Location(
        id=device_id,
        name=location_in.name,
        tag=location_in.tag,
        geom=from_shape(Point(location_in.lon, location_in.lat), srid=4326)
    )
    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)
    
    # Manually attach lat/lon for schema
    point = to_shape(db_obj.geom)
    db_obj.lat = point.y
    db_obj.lon = point.x
    return db_obj


def read_location(
    *,
    db: Session,
    id: str,
) -> Any:
    """
    Internal helper to retrieve details of a specific device.
    """
    db_obj = db.query(models.Location).filter(models.Location.id == id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Device not found")
    
    if db_obj.geom:
        point = to_shape(db_obj.geom)
        db_obj.lat = point.y
        db_obj.lon = point.x
    return db_obj


@router.get("/", response_model=List[schemas.Location], summary="List all devices")
def read_locations(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve a list of all registered devices with their coordinates.
    """
    locations = db.query(models.Location).offset(skip).limit(limit).all()
    for loc in locations:
        if loc.geom:
            point = to_shape(loc.geom)
            loc.lat = point.y
            loc.lon = point.x
    return locations

@router.put("/{id}", response_model=schemas.Location, summary="Update device")
def update_location(
    *,
    db: Session = Depends(get_db),
    id: str,
    location_in: schemas.LocationUpdate,
) -> Any:
    """
    Update device settings (name, tag, location).
    """
    db_obj = db.query(models.Location).filter(models.Location.id == id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Device not found")
    
    update_data = location_in.model_dump(exclude_unset=True)
    if "lat" in update_data or "lon" in update_data:
        lat = update_data.pop("lat", None)
        lon = update_data.pop("lon", None)
        
        # Get current values if one is missing
        current_point = to_shape(db_obj.geom) if db_obj.geom else Point(0, 0)
        new_lat = lat if lat is not None else current_point.y
        new_lon = lon if lon is not None else current_point.x
        db_obj.geom = from_shape(Point(new_lon, new_lat), srid=4326)

    for field in update_data:
        setattr(db_obj, field, update_data[field])

    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)
    
    if db_obj.geom:
        point = to_shape(db_obj.geom)
        db_obj.lat = point.y
        db_obj.lon = point.x
    return db_obj

@router.delete("/{id}", response_model=schemas.Location, summary="Delete device")
def delete_location(
    *,
    db: Session = Depends(get_db),
    id: str,
) -> Any:
    """
    Delete a device by its ID.
    """
    db_obj = db.query(models.Location).filter(models.Location.id == id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Note: Peaks associated with this device might cause foreign key constraint error 
    # if not handled (on delete cascade). 
    db.delete(db_obj)
    db.commit()
    return db_obj

@router.post("/{id}/init", response_model=schemas.DeviceInitResponse, summary="Initialize device and sync time")
def init_device(
    *,
    db: Session = Depends(get_db),
    id: str
) -> Any:
    """
    Initializes a device and returns the current server time for synchronization.
    If the device is not registered, it returns 404.
    """
    db_obj = db.query(models.Location).filter(models.Location.id == id).first()
    if not db_obj:
        raise HTTPException(status_code=404, detail="Device not found")
    
    return {
        "device_id": id,
        "server_time": datetime.now()
    }

