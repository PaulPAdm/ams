from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
import os
from app.api.v1.api import api_router
from app.core.config import settings
from app.db.session import engine
from app.models import Base

from app.udp_server.server import start_udp_server_thread

Base.metadata.create_all(bind=engine)

import logging
import sys

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)

from contextlib import asynccontextmanager

logger = logging.getLogger(__name__)

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info(f"Starting UDP server on {settings.UDP_SERVER_HOST}:{settings.UDP_SERVER_PORT}...")
    # Start UDP server in a separate thread with config parameters
    start_udp_server_thread(host=settings.UDP_SERVER_HOST, port=settings.UDP_SERVER_PORT)
    logger.info("UDP server thread started successfully")
    yield

app = FastAPI(
    title=settings.PROJECT_NAME,
    description="Acoustic monitoring backend with UDP audio ingestion, Whisper analysis, and incident tracking",
    version="1.0.0",
    openapi_url=f"{settings.API_V1_STR}/openapi.json",
    docs_url=f"{settings.API_V1_STR}/docs",
    redoc_url=f"{settings.API_V1_STR}/redoc",
    lifespan=lifespan
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
