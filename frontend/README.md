# Frontend

Frontend City Audio Monitor to aplikacja React/Vite dla operatora systemu. Pokazuje czujniki i incydenty na mapie oraz udostępnia panel administracyjny.

## Technologie

- React 19
- Vite
- Mapbox GL
- react-map-gl
- lucide-react
- ESLint
- Nginx w obrazie produkcyjnym

## Widoki

Frontend składa się z dwóch głównych części:

- widok operacyjny z mapą, markerami czujników, markerami incydentów i powiadomieniami,
- panel administracyjny do zarządzania urządzeniami oraz przeglądania danych systemowych.

W Dockerze oba widoki są serwowane przez ten sam frontend:

```text
/        # widok operatora
/admin/  # panel administracyjny
```

## Struktura

```text
frontend/
├── src/
│   ├── app/        # konfiguracja, style i przełączanie widoków
│   ├── entities/   # API i modele domenowe
│   ├── features/   # logika funkcji administracyjnych
│   ├── shared/     # wspólne API, formatowanie i UI
│   └── widgets/    # większe moduły ekranu
├── public/
├── Dockerfile
├── nginx.conf
└── package.json
```

## Uruchomienie lokalne

```powershell
cd frontend
npm install
npm run dev
```

Domyślnie Vite uruchamia aplikację pod:

```text
http://localhost:5173
```

Jeśli backend działa poza gatewayem, ustaw:

```powershell
$env:VITE_API_BASE_URL="http://localhost:8000"
```

## Uruchomienie produkcyjne

Z katalogu głównego projektu:

```powershell
docker compose up --build
```

Gateway wystawia frontend pod:

```text
http://localhost/
```

## Konfiguracja

Najważniejsze zmienne Vite:

```text
VITE_APP_TITLE
VITE_APP_HTML_TITLE
VITE_APP_LOCALE
VITE_API_BASE_URL
VITE_MAPBOX_TOKEN
VITE_MAP_STYLE
VITE_MAP_CENTER_LAT
VITE_MAP_CENTER_LON
VITE_MAP_ZOOM
VITE_SENSORS_FETCH_LIMIT
VITE_INCIDENTS_FETCH_LIMIT
VITE_INCIDENTS_POLLING_MS
VITE_NOTIFICATION_LIMIT
VITE_NOTIFICATION_TTL_MS
VITE_ADMIN_REFRESH_MS
```

Do poprawnego wyświetlania mapy potrzebny jest `VITE_MAPBOX_TOKEN`.

## Komendy

```powershell
npm run dev      # serwer developerski
npm run build    # build produkcyjny
npm run lint     # statyczna kontrola kodu
npm run preview  # podgląd buildu
```

## Dane pobierane z backendu

Frontend korzysta głównie z:

- `GET /api/devices` - lista czujników i ich ostatni status,
- `GET /api/suspicious_incidents` - lista incydentów na mapie,
- `GET /api/devices/{device_id}/sound-events` - zdarzenia wybranego czujnika,
- `POST /api/devices/{device_id}/sound-events/{event_id}/audio` - upload pliku audio dla zdarzenia,
- `GET /api/health` - stan backendu.

## Uwagi

- Widok operatora jest map-first: mapa jest główną powierzchnią pracy.
- Powiadomienia o nowych incydentach są generowane po stronie frontendu na podstawie cyklicznego odświeżania danych.
- Panel admina jest częścią tej samej aplikacji, a nie osobnym frontendem.
