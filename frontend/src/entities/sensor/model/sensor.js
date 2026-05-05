const fallbackSensorsRaw = [
  { id: 'node-01', lat: 52.222, lon: 21.007, name: 'Node 01 - Politechnika', battery: 85, tag: 'PW' },
  { id: 'node-02', lat: 52.225, lon: 21.015, name: 'Node 02 - City Center', battery: 92, tag: 'CTR' },
  { id: 'node-03', lat: 52.218, lon: 21.012, name: 'Node 03 - Plac Zbawiciela', battery: 45, tag: 'PZ' },
  { id: 'node-04', lat: 52.228, lon: 21.002, name: 'Node 04 - Central Station', battery: 78, tag: 'CNTR' },
  { id: 'node-05', lat: 52.215, lon: 21, name: 'Node 05 - Koszyki', battery: 12, tag: 'KSZ' },
];

const clampBattery = (value, fallback) => {
  const parsed = Number(value);

  if (!Number.isFinite(parsed)) {
    return fallback;
  }

  return Math.min(100, Math.max(0, Math.round(parsed)));
};

export function mapSensor(rawSensor, index = 0) {
  const fallbackBattery = Math.max(28, 94 - index * 12);
  const battery = clampBattery(rawSensor.battery, fallbackBattery);
  const lat = Number(rawSensor.lat);
  const lng = Number(rawSensor.lon ?? rawSensor.lng);

  return {
    id: String(rawSensor.id ?? `sensor-${index + 1}`),
    name: rawSensor.name || `Acoustic node ${index + 1}`,
    tag: rawSensor.tag || null,
    lat,
    lng,
    battery,
    status: rawSensor.status || (battery <= 20 ? 'Low Battery' : 'Online'),
    order: index + 1,
  };
}

export const FALLBACK_SENSORS = fallbackSensorsRaw.map(mapSensor);

export function isSensorLocated(sensor) {
  return Number.isFinite(sensor.lat) && Number.isFinite(sensor.lng);
}

export function getSensorTone(sensor) {
  if (sensor.status !== 'Online' || sensor.battery <= 20) {
    return 'warning';
  }

  return 'ok';
}

export function formatSensorCode(sensor) {
  if (sensor.tag) {
    return sensor.tag.toUpperCase();
  }

  const compactId = String(sensor.id).replace(/[^a-z0-9]/gi, '').toUpperCase();

  if (/^\d+$/.test(compactId)) {
    return `NODE-${compactId.padStart(3, '0')}`;
  }

  return compactId.slice(0, 10) || 'NODE';
}
