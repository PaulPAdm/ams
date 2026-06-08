import { toBoolOrNull } from '@/shared/lib/format';

const clampBattery = (value) => {
  const parsed = Number(value);

  if (!Number.isFinite(parsed)) {
    return null;
  }

  return Math.min(100, Math.max(0, Math.round(parsed)));
};

const toNumberOrNull = (value) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
};

const nsToIso = (value) => {
  if (value === null || value === undefined || value === '') {
    return null;
  }

  try {
    const milliseconds = BigInt(String(value)) / 1_000_000n;
    return new Date(Number(milliseconds)).toISOString();
  } catch {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? new Date(parsed / 1_000_000).toISOString() : null;
  }
};

const mapLatestHealth = (rawHealth) => {
  if (!rawHealth) {
    return null;
  }

  return {
    receivedAt: nsToIso(rawHealth.received_at_ns),
    statusMessage: rawHealth.status_message || null,
    busVoltageV: toNumberOrNull(rawHealth.bus_voltage_v),
    shuntVoltageMv: toNumberOrNull(rawHealth.shunt_voltage_mv),
    currentMa: toNumberOrNull(rawHealth.current_ma),
    powerMw: toNumberOrNull(rawHealth.power_mw),
    computedPowerMw: toNumberOrNull(rawHealth.computed_power_mw),
    wifiConnected: rawHealth.wifi_connected,
    microphoneActive: rawHealth.microphone_active,
    ina219Online: toBoolOrNull(rawHealth.ina219_online),
    audioQueueDepth: toNumberOrNull(rawHealth.audio_queue_depth),
    audioDroppedChunks: toNumberOrNull(rawHealth.audio_dropped_chunks),
  };
};

const mapLatestTimeSync = (rawTimeSync) => {
  if (!rawTimeSync) {
    return null;
  }

  return {
    id: Number(rawTimeSync.id),
    createdAt: nsToIso(rawTimeSync.created_at_ns),
    serverReceivedAt: nsToIso(rawTimeSync.server_received_epoch_ns),
    serverTransmitAt: nsToIso(rawTimeSync.server_transmit_epoch_ns),
  };
};

export function mapSensor(rawSensor, index = 0) {
  const battery = clampBattery(rawSensor.battery);
  const lat = Number(rawSensor.lat);
  const lng = Number(rawSensor.lon ?? rawSensor.lng);

  return {
    id: String(rawSensor.id ?? `sensor-${index + 1}`),
    name: rawSensor.name || `Acoustic node ${index + 1}`,
    tag: rawSensor.tag || null,
    lat,
    lng,
    battery,
    status: rawSensor.status || (battery !== null && battery <= 20 ? 'Low Battery' : 'Registered'),
    order: index + 1,
    latestHealth: mapLatestHealth(rawSensor.latest_health),
    latestTimeSync: mapLatestTimeSync(rawSensor.latest_time_sync),
  };
}

export function isSensorLocated(sensor) {
  return Number.isFinite(sensor.lat) && Number.isFinite(sensor.lng);
}

export function getSensorTone(sensor) {
  if (sensor.battery !== null && sensor.battery <= 20) {
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

export function getSensorLocationSummary(sensor) {
  if (sensor.name && sensor.tag) {
    return `${sensor.name} (${sensor.tag})`;
  }

  return sensor.name || sensor.tag || 'Registered acoustic device';
}
