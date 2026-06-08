# Backend

Backend City Audio Monitor to aplikacja FastAPI odpowiedzialna za przyjmowanie danych z urządzeń, zapisywanie ich w PostgreSQL/PostGIS oraz udostępnianie danych frontendowi.

## Technologie

- Python 3.11
- FastAPI
- SQLAlchemy 2
- PostgreSQL + PostGIS
- GeoAlchemy2
- Shapely, PyProj, SciPy, NumPy
- Uvicorn
- Poetry

## Zakres odpowiedzialności

Backend obsługuje:

- rejestrację i zarządzanie urządzeniami,
- przechowywanie lokalizacji urządzeń w PostGIS,
- synchronizację czasu urządzeń,
- raporty stanu urządzeń,
- zdarzenia dźwiękowe wykryte lokalnie,
- upload krótkich fragmentów audio,
- grupowanie peaków i triangulację TDOA,
- incydenty podejrzane widoczne we frontendzie.

## Struktura

```text
backend/
├── app/
│   ├── api/v1/endpoints/    # endpointy FastAPI
│   ├── core/                # konfiguracja i narzędzia czasu
│   ├── db/                  # sesja bazy i tworzenie schematu
│   ├── models/              # modele SQLAlchemy
│   ├── schemas/             # schematy Pydantic
│   ├── services/            # logika triangulacji
│   └── main.py              # punkt startowy aplikacji
├── storage/audio/           # lokalny storage audio
├── Dockerfile
└── pyproject.toml
```

## Uruchomienie w Dockerze

Najprościej uruchomić backend jako część całego stacka z katalogu głównego projektu:

```powershell
docker compose up --build
```

Backend będzie dostępny przez gateway:

```text
http://localhost/api
```

Dokumentacja OpenAPI:

```text
http://localhost/api/docs
```

## Uruchomienie lokalne

Wymagania:

- Python 3.11+
- Poetry
- działający PostgreSQL z PostGIS

Przykład:

```powershell
cd backend
poetry install
$env:DATABASE_URL="postgresql://postgres:postgres@localhost:5432/fastapi_db"
poetry run uvicorn app.main:app --reload
```

Lokalnie backend działa domyślnie pod:

```text
http://127.0.0.1:8000
```

## Konfiguracja

Najważniejsze zmienne środowiskowe:

```text
DATABASE_URL
AUDIO_STORAGE_PATH
CORS_ALLOW_ORIGINS
```

W Docker Compose `DATABASE_URL` jest ustawiany automatycznie na usługę `db`.

## Endpointy

Główny prefiks API to `/api`.

| Obszar | Endpointy |
| --- | --- |
| Health | `GET /api/health` |
| Urządzenia | `/api/devices` |
| Synchronizacja czasu | `/api/devices/{device_id}/time/sync` |
| Raporty health | `/api/devices/{device_id}/health` |
| Zdarzenia dźwiękowe | `/api/devices/{device_id}/sound-events` |
| Audio zdarzenia | `/api/devices/{device_id}/sound-events/{event_id}/audio` |
| Incydenty | `/api/suspicious_incidents` |

## Model przepływu danych

1. Urządzenie rejestruje się lub jest dodawane przez panel admina.
2. Urządzenie synchronizuje czas z backendem.
3. Urządzenie cyklicznie wysyła health report.
4. Po wykryciu impulsu urządzenie wysyła `SoundEvent`.
5. Backend tworzy `Peak` i próbuje pogrupować świeże peaki z kilku urządzeń.
6. Jeśli są co najmniej trzy urządzenia w krótkim oknie czasu, backend wykonuje triangulację.
7. Wynik triangulacji jest zapisywany jako `SuspiciousIncident`.
8. Frontend pobiera urządzenia, statusy i incydenty przez API.

## Baza danych

Backend tworzy brakujące tabele przez `Base.metadata.create_all(bind=engine)`. Nie wykonuje automatycznego dropowania tabel przy starcie.

Lokalizacje urządzeń i incydentów są przechowywane jako geometria PostGIS w układzie WGS84. Do obliczeń triangulacji współrzędne są konwertowane do układu metrycznego.

## Audio

Fragmenty audio dla zdarzeń są zapisywane w:

```text
storage/audio/events
```

Backend przyjmuje:

- normalny plik audio przez `multipart/form-data` w polu `file` (`.wav`, `.mp3`, `.ogg`, `.flac`, `.m4a`, `.aac`, `.webm`),
- raw `PCM16LE` z urządzenia jako body requestu; backend zapisuje go jako plik `.wav`,
- pobieranie audio przez `/api/devices/{device_id}/sound-events/{event_id}/audio/download`.
