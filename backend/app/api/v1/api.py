from fastapi import APIRouter
from app.api.v1.endpoints import devices, peaks, sound_events, suspicious_incidents

api_router = APIRouter()
api_router.include_router(suspicious_incidents.router, prefix="/suspicious_incidents", tags=["suspicious_incidents"])
api_router.include_router(peaks.router, prefix="/peaks", tags=["peaks"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(sound_events.router, prefix="/devices", tags=["sound-events"])
