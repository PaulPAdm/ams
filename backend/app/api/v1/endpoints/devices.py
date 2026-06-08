import logging
import uuid
from typing import Any, List

from fastapi import APIRouter, Depends, HTTPException
from geoalchemy2.shape import from_shape, to_shape
from shapely.geometry import Point
from sqlalchemy.orm import Session

from app.core.time_utils import WARSAW_TIMEZONE_NAME, epoch_ns, warsaw_offset_seconds
from app.db.session import get_db
from app import schemas, models

router = APIRouter()
logger = logging.getLogger(__name__)


def _require_device(db: Session, device_id: str) -> models.Location:
    device = db.query(models.Location).filter(models.Location.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


def _attach_coordinates(device: models.Location) -> models.Location:
    if device.geom:
        point = to_shape(device.geom)
        device.lat = point.y
        device.lon = point.x
    return device


def _latest_device_health_report(db: Session, device_id: str) -> models.DeviceHealthReport | None:
    return (
        db.query(models.DeviceHealthReport)
        .filter(models.DeviceHealthReport.device_id == device_id)
        .order_by(models.DeviceHealthReport.received_at_ns.desc())
        .first()
    )


def _latest_device_time_sync_event(db: Session, device_id: str) -> models.DeviceTimeSyncEvent | None:
    return (
        db.query(models.DeviceTimeSyncEvent)
        .filter(models.DeviceTimeSyncEvent.device_id == device_id)
        .order_by(models.DeviceTimeSyncEvent.created_at_ns.desc())
        .first()
    )


def _attach_status(db: Session, device: models.Location) -> models.Location:
    _attach_coordinates(device)
    device.latest_health = _latest_device_health_report(db, device.id)
    device.latest_time_sync = _latest_device_time_sync_event(db, device.id)
    return device


def _create_device_time_sync_event(
    *,
    db: Session,
    device_id: str,
    client_sent_monotonic_ns: int,
    server_received_epoch_ns: int,
    server_transmit_epoch_ns: int,
) -> models.DeviceTimeSyncEvent:
    db_obj = models.DeviceTimeSyncEvent(
        device_id=device_id,
        client_sent_monotonic_ns=client_sent_monotonic_ns,
        server_received_epoch_ns=server_received_epoch_ns,
        server_transmit_epoch_ns=server_transmit_epoch_ns,
    )
    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)
    return db_obj


def _create_device_health_report(
    *,
    db: Session,
    device_id: str,
    report_in: schemas.DeviceHealthReportCreate,
) -> models.DeviceHealthReport:
    _require_device(db, device_id)

    db_obj = models.DeviceHealthReport(
        device_id=device_id,
        **report_in.model_dump()
    )
    db.add(db_obj)
    db.commit()
    db.refresh(db_obj)

    logger.info(
        "Health report from %s: wifi=%s mic=%s ina219=%s uptime_ms=%s bus_v=%s current_ma=%s power_mw=%s queue_depth=%s dropped=%s msg=%s",
        device_id,
        db_obj.wifi_connected,
        db_obj.microphone_active,
        db_obj.ina219_online,
        db_obj.uptime_ms,
        db_obj.bus_voltage_v,
        db_obj.current_ma,
        db_obj.power_mw,
        db_obj.audio_queue_depth,
        db_obj.audio_dropped_chunks,
        db_obj.status_message,
    )

    return db_obj

@router.post("/", response_model=schemas.Location, summary="Register device")
def create_device(
    *,
    db: Session = Depends(get_db),
    location_in: schemas.LocationCreate,
) -> Any:
    """
    Registers a new device (microphone).
    """
    device_id = location_in.id or str(uuid.uuid4())
    existing = db.query(models.Location).filter(models.Location.id == device_id).first()
    if existing:
        raise HTTPException(status_code=400, detail=f"Device with ID {device_id} already exists.")

    device = models.Location(
        id=device_id,
        name=location_in.name,
        tag=location_in.tag,
        geom=from_shape(Point(location_in.lon, location_in.lat), srid=4326),
    )
    db.add(device)
    db.commit()
    db.refresh(device)
    return _attach_status(db, device)


@router.get("/{device_id}", response_model=schemas.Location, summary="Get device by ID")
def read_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Retrieve details of a specific device.
    """
    return _attach_status(db, _require_device(db, device_id))


@router.get("/", response_model=List[schemas.Location], summary="List all devices")
def list_devices(
    db: Session = Depends(get_db),
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Retrieve a list of all registered devices with their coordinates.
    """
    devices = db.query(models.Location).offset(skip).limit(limit).all()
    return [_attach_status(db, device) for device in devices]

@router.put("/{device_id}", response_model=schemas.Location, summary="Update device")
def update_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    location_in: schemas.LocationUpdate,
) -> Any:
    """
    Update device settings (name, tag, location).
    """
    device = _require_device(db, device_id)
    update_data = location_in.model_dump(exclude_unset=True)

    if "lat" in update_data or "lon" in update_data:
        lat = update_data.pop("lat", None)
        lon = update_data.pop("lon", None)
        current_point = to_shape(device.geom) if device.geom else Point(0, 0)
        device.geom = from_shape(
            Point(lon if lon is not None else current_point.x, lat if lat is not None else current_point.y),
            srid=4326,
        )

    for field, value in update_data.items():
        setattr(device, field, value)

    db.add(device)
    db.commit()
    db.refresh(device)
    return _attach_status(db, device)

@router.delete("/{device_id}", response_model=schemas.Location, summary="Delete device")
def delete_device(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Delete a device by its ID.
    """
    device = _require_device(db, device_id)
    db.query(models.SoundEvent).filter(models.SoundEvent.device_id == device_id).delete(synchronize_session=False)
    db.query(models.Peak).filter(models.Peak.device_id == device_id).delete(synchronize_session=False)
    db.query(models.DeviceHealthReport).filter(models.DeviceHealthReport.device_id == device_id).delete(synchronize_session=False)
    db.query(models.DeviceTimeSyncEvent).filter(models.DeviceTimeSyncEvent.device_id == device_id).delete(synchronize_session=False)
    db.delete(device)
    db.commit()
    return _attach_coordinates(device)

@router.get("/{device_id}/time/sync", response_model=schemas.DeviceTimeSyncResponse, summary="Synchronize device clock")
def sync_device_time(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    client_sent_monotonic_ns: int,
) -> Any:
    """
    Return server-side timestamps for NTP-style device clock synchronization.
    """
    server_received_epoch_ns = epoch_ns()
    _require_device(db, device_id)
    server_transmit_epoch_ns = epoch_ns()

    _create_device_time_sync_event(
        db=db,
        device_id=device_id,
        client_sent_monotonic_ns=client_sent_monotonic_ns,
        server_received_epoch_ns=server_received_epoch_ns,
        server_transmit_epoch_ns=server_transmit_epoch_ns,
    )

    return {
        "device_id": device_id,
        "server_received_epoch_ns": server_received_epoch_ns,
        "server_transmit_epoch_ns": server_transmit_epoch_ns,
        "timezone_name": WARSAW_TIMEZONE_NAME,
        "timezone_offset_seconds": warsaw_offset_seconds(server_transmit_epoch_ns),
    }


@router.get("/{device_id}/time/sync/latest", response_model=schemas.DeviceTimeSyncEvent, summary="Get latest device clock synchronization")
def read_latest_device_time_sync_event(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Return the most recent time synchronization event for a device.
    """
    _require_device(db, device_id)
    event = _latest_device_time_sync_event(db, device_id)
    if not event:
        raise HTTPException(status_code=404, detail="No time sync events found for this device")
    return event


@router.post("/{device_id}/health", response_model=schemas.DeviceHealthReport, summary="Report device health")
def create_device_health_report(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    report_in: schemas.DeviceHealthReportCreate,
) -> Any:
    """
    Accept a structured health report from an embedded device.
    """
    return _create_device_health_report(db=db, device_id=device_id, report_in=report_in)


@router.get("/{device_id}/health/report", response_model=schemas.DeviceHealthReport, summary="Report device health via query params")
def create_device_health_report_from_query(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    report_in: schemas.DeviceHealthReportCreate = Depends(),
) -> Any:
    """
    Accept a health report using query parameters for constrained clients such as Pico.
    """
    return _create_device_health_report(db=db, device_id=device_id, report_in=report_in)


@router.get("/{device_id}/health/latest", response_model=schemas.DeviceHealthReport, summary="Get latest device health report")
def read_latest_device_health_report(
    *,
    db: Session = Depends(get_db),
    device_id: str,
) -> Any:
    """
    Return the most recent health report received from a device.
    """
    _require_device(db, device_id)
    report = _latest_device_health_report(db, device_id)
    if not report:
        raise HTTPException(status_code=404, detail="No health reports found for this device")
    return report


@router.get("/{device_id}/peaks", response_model=List[schemas.Peak], summary="List device peaks")
def list_device_peaks(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    skip: int = 0,
    limit: int = 1000,
) -> Any:
    _require_device(db, device_id)
    return (
        db.query(models.Peak)
        .filter(models.Peak.device_id == device_id)
        .order_by(models.Peak.peak_time_ns.desc())
        .offset(skip)
        .limit(limit)
        .all()
    )


@router.get("/{device_id}/health", response_model=List[schemas.DeviceHealthReport], summary="List device health reports")
def list_device_health_reports(
    *,
    db: Session = Depends(get_db),
    device_id: str,
    skip: int = 0,
    limit: int = 100,
) -> Any:
    """
    Return recent health reports received from a device.
    """
    _require_device(db, device_id)
    return (
        db.query(models.DeviceHealthReport)
        .filter(models.DeviceHealthReport.device_id == device_id)
        .order_by(models.DeviceHealthReport.received_at_ns.desc())
        .offset(skip)
        .limit(limit)
        .all()
    )
