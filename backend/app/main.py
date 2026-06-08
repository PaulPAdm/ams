import logging
import os
import sys

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from app.api.v1.api import api_router
from app.core.config import settings
from app.db.schema import create_database_schema

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)

create_database_schema()

logger = logging.getLogger(__name__)

app = FastAPI(
    title=settings.PROJECT_NAME,
    description="Acoustic monitoring backend with event-based audio uploads, time sync, and incident triangulation",
    version="1.0.0",
    openapi_url=f"{settings.API_V1_STR}/openapi.json",
    docs_url=f"{settings.API_V1_STR}/docs",
    redoc_url=f"{settings.API_V1_STR}/redoc",
)

# CORS configuration
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_allow_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Mount static files for audio storage
if not os.path.exists(settings.AUDIO_STORAGE_PATH):
    os.makedirs(settings.AUDIO_STORAGE_PATH, exist_ok=True)
app.mount("/storage/audio", StaticFiles(directory=settings.AUDIO_STORAGE_PATH), name="audio")

app.include_router(api_router, prefix=settings.API_V1_STR)

@app.get("/")
def read_root():
    return {"message": "AMS backend is running"}

@app.get(f"{settings.API_V1_STR}/health")
def healthcheck():
    return {"status": "ok"}
