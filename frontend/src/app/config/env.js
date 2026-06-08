const env = import.meta.env;

const toNumber = (value, fallback) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
};

export const appConfig = {
  htmlTitle: env.VITE_APP_HTML_TITLE || env.VITE_APP_TITLE || 'Urban Acoustic Grid',
  appName: env.VITE_APP_TITLE || 'Acoustic Monitoring System',
  appShortName: env.VITE_APP_SHORT_NAME || 'AMS',
  locale: env.VITE_APP_LOCALE || 'en-GB',
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
    adminRefreshMs: toNumber(env.VITE_ADMIN_REFRESH_MS, 60000),
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
