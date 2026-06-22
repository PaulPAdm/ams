# City Audio Monitor

> **Student project.** This repository was created for educational purposes as part of university coursework. It is not a production-grade system and is not intended for real deployment.

City Audio Monitor is an IoT system for detecting suspicious acoustic events in urban spaces. The project consists of embedded devices with microphones, a backend API, a PostGIS database, and a web application for the operator. The target deployment runs the services on a VPS in Docker containers behind an Nginx gateway.

## Core idea

Measurement devices listen to their surroundings locally and send only relevant events to the server, instead of streaming continuous audio. The backend stores events, device status, and time-synchronization information. When several devices register the same impulse within a short time window, the backend can estimate the approximate location of the sound source using the TDOA (Time Difference of Arrival) method.

The frontend shows devices and incidents on a map and provides an admin panel for managing sensors and system data.

## Repository structure

```text
ams/
├── backend/    # FastAPI, SQLAlchemy, PostGIS, event and triangulation logic
├── frontend/   # React + Vite, operator map and admin panel
├── embeded/    # Raspberry Pi Pico 2 W firmware
├── docs/       # PDF reports and PlantUML/SVG diagrams
├── docker-compose.yml
└── nginx.conf
```

The `presentation/` directory contains supporting materials and is not a core part of the current system architecture.

## Architecture

Main components:

- `embeded` — Pico 2 W device with an I2S microphone, an e-paper display, INA219 power monitoring, and an audio buffer.
- `backend` — FastAPI API handling devices, health reports, time synchronization, sound events, and incidents.
- `db` — PostgreSQL with the PostGIS extension for storing device and incident locations.
- `frontend` — web application with a map, sensor markers, incident notifications, and an admin panel.
- `gateway` — Nginx routing traffic to the backend and frontend.

Architecture diagrams are available in [docs/diagrams](docs/diagrams).

## Quick start

Requirements:

- Docker and Docker Compose
- a free port `80`
- a Mapbox token for full map functionality

Start the whole stack:

```powershell
docker compose up --build
```

After startup:

- web application: `http://localhost/`
- admin panel: `http://localhost/admin/`
- backend health check: `http://localhost/api/health`
- OpenAPI documentation: `http://localhost/api/docs`

The default database is created in the `postgis/postgis:15-3.3` container. The backend creates missing tables on startup without automatically dropping existing data.

## Configuration

Key variables:

```text
POSTGRES_USER
POSTGRES_PASSWORD
POSTGRES_DB
DATABASE_URL
HTTP_PUBLISHED_PORT
VITE_MAPBOX_TOKEN
VITE_API_BASE_URL
VITE_MAP_CENTER_LAT
VITE_MAP_CENTER_LON
VITE_MAP_ZOOM
```

In Docker mode, most database settings are defined in `docker-compose.yml`. To run the backend locally outside Docker, you must provide `DATABASE_URL`.

## Main API

The backend is served under the `/api` prefix.

- `GET /api/health` — backend liveness check.
- `/api/devices` — register, list, edit, and delete devices.
- `/api/devices/{device_id}/time/sync` — device time synchronization.
- `/api/devices/{device_id}/health` — device health reports.
- `/api/devices/{device_id}/sound-events` — sound events detected locally.
- `/api/devices/{device_id}/sound-events/{event_id}/audio` — upload an audio fragment for an event.
- `/api/suspicious_incidents` — list and manage incidents.

Request/response field details are available in the Swagger UI at `/api/docs`.

## Documentation

Project documentation is available in [docs](docs):

- PDF reports from the successive stages of the work,
- general architecture diagram,
- embedded device logic diagram,
- backend logic diagram,
- PlantUML source files.

## Working on individual components

Documentation for the individual parts:

- [backend/README.md](backend/README.md)
- [frontend/README.md](frontend/README.md)
- [embeded/README.md](embeded/README.md)
- [docs/README.md](docs/README.md)

## Design notes

- The system favors an event-first flow: a device sends an event and an optional audio fragment, rather than a constant stream.
- Time synchronization is important for TDOA triangulation.
- A VPS is a natural deployment target, since the frontend, backend, database, and gateway can run as a single Docker Compose stack.
- A mobile application could use the same API model as the embedded devices.

## License

This project is licensed under the [CRAPL](LICENSE) (Community Research and Academic Programming License) — a license tailored to academic and student code. The software is provided "as is", without warranty of any kind, and you agree to hold the author free from shame, embarrassment, or ridicule for any hacks or kludges found within.
