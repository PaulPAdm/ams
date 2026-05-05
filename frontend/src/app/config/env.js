const env = import.meta.env;

const toNumber = (value, fallback) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const toBoolean = (value, fallback) => {
  if (typeof value !== 'string') {
    return fallback;
  }

  if (value.toLowerCase() === 'true') {
    return true;
  }

  if (value.toLowerCase() === 'false') {
    return false;
  }

  return fallback;
};

export const appConfig = {
  appTitle: env.VITE_APP_TITLE || 'Urban Acoustic Grid',
  htmlTitle: env.VITE_APP_HTML_TITLE || env.VITE_APP_TITLE || 'Urban Acoustic Grid',
  locale: env.VITE_APP_LOCALE || 'en-GB',
  cityLabel: env.VITE_APP_CITY_LABEL || 'Warsaw coverage',
  apiBaseUrl: env.VITE_API_BASE_URL || '',
  mapboxToken: env.VITE_MAPBOX_TOKEN || '',
  mapStyle: env.VITE_MAP_STYLE || 'mapbox://styles/mapbox/dark-v11',
  mapView: {
    latitude: toNumber(env.VITE_MAP_CENTER_LAT, 52.222),
    longitude: toNumber(env.VITE_MAP_CENTER_LON, 21.0122),
    zoom: toNumber(env.VITE_MAP_ZOOM, 13.8),
  },
  limits: {
    sensors: toNumber(env.VITE_SENSORS_FETCH_LIMIT, 300),
    incidents: toNumber(env.VITE_INCIDENTS_FETCH_LIMIT, 100),
  },
  intervals: {
    incidentsMs: toNumber(env.VITE_INCIDENTS_POLLING_MS, 10000),
    telemetryMs: toNumber(env.VITE_AUDIO_LEVEL_INTERVAL_MS, 1200),
    adminRefreshMs: toNumber(env.VITE_ADMIN_REFRESH_MS, 60000),
  },
  simulation: {
    enabledByDefault: toBoolean(env.VITE_SIMULATION_ENABLED, false),
    historyLimit: toNumber(env.VITE_SIMULATION_HISTORY_LIMIT, 6),
    minConfidence: toNumber(env.VITE_SIMULATION_MIN_CONFIDENCE, 82),
    maxConfidence: toNumber(env.VITE_SIMULATION_MAX_CONFIDENCE, 97),
    minDb: toNumber(env.VITE_LIVE_DB_MIN, 41),
    maxDb: toNumber(env.VITE_LIVE_DB_MAX, 76),
  },
  alerts: {
    maxNotifications: toNumber(env.VITE_NOTIFICATION_LIMIT, 4),
    dismissMs: toNumber(env.VITE_NOTIFICATION_TTL_MS, 5000),
  },
  admin: {
    limits: {
      devices: toNumber(env.VITE_ADMIN_DEVICES_FETCH_LIMIT, 1000),
      peaks: toNumber(env.VITE_ADMIN_PEAKS_FETCH_LIMIT, 1000),
      audioRecords: toNumber(env.VITE_ADMIN_AUDIO_RECORDS_FETCH_LIMIT, 1000),
    },
  },
};

export const isMapConfigured = Boolean(appConfig.mapboxToken);
