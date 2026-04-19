from fastapi import APIRouter
from app.api.v1.endpoints import locations, peaks, suspicious_incidents, audio_records, devices

api_router = APIRouter()
api_router.include_router(locations.router, prefix="/locations", tags=["locations"])
api_router.include_router(peaks.router, prefix="/peaks", tags=["peaks"])
api_router.include_router(suspicious_incidents.router, prefix="/suspicious_incidents", tags=["suspicious_incidents"])
api_router.include_router(audio_records.router, prefix="/audio_records", tags=["audio_records"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
