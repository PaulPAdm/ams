from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    PROJECT_NAME: str = "FastAPI PostGIS App"
    API_V1_STR: str = "/api"
    DATABASE_URL: str
    UDP_SERVER_HOST: str = "0.0.0.0"
    UDP_SERVER_PORT: int = 5000

    # Audio Settings
    AUDIO_SAMPLE_RATE: int = 16000
    AUDIO_CHANNELS: int = 1
    AUDIO_BIT_DEPTH: int = 16
    AUDIO_STORAGE_PATH: str = "storage/audio"

    # Whisper Settings
    WHISPER_API_URL: str = "http://localhost:9000"
    HELP_KEYWORDS: list[str] = [
        "help", "pomocy", "ratunku",
        "kill", "murder", "rape", "fire", "attack",
        "zabijają", "morderstwo", "gwałt", "pożar", "atak"
    ]
    AUDIO_ANALYSIS_INTERVAL_SEC: int = 30
    AUDIO_ANALYSIS_WINDOW_SEC: int = 60
    CORS_ALLOW_ORIGINS: str = (
        "http://localhost,"
        "http://127.0.0.1,"
        "http://localhost:80,"
        "http://127.0.0.1:80,"
        "http://localhost:8080,"
        "http://127.0.0.1:8080,"
        "http://localhost:5173,"
        "http://127.0.0.1:5173,"
        "http://localhost:5174,"
        "http://127.0.0.1:5174"
    )

    @property
    def cors_allow_origins(self) -> list[str]:
        return [origin.strip() for origin in self.CORS_ALLOW_ORIGINS.split(",") if origin.strip()]

    model_config = {
        "env_file": ".env",
        "case_sensitive": True,
        "extra": "ignore"
    }

settings = Settings()
