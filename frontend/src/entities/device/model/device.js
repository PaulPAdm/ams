import { toBoolOrNull } from '@/shared/lib/format';

const toCoordinate = (value) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
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

export function mapDeviceHealth(rawHealth) {
  if (!rawHealth) {
    return null;
  }

  return {
    receivedAt: nsToIso(rawHealth.received_at_ns),
    firmwareVersion: rawHealth.firmware_version || null,
    statusMessage: rawHealth.status_message || null,
    uptimeMs: toNumberOrNull(rawHealth.uptime_ms),
    wifiConnected: rawHealth.wifi_connected,
    microphoneActive: rawHealth.microphone_active,
    ina219Online: toBoolOrNull(rawHealth.ina219_online),
    busVoltageV: toNumberOrNull(rawHealth.bus_voltage_v),
    shuntVoltageMv: toNumberOrNull(rawHealth.shunt_voltage_mv),
    currentMa: toNumberOrNull(rawHealth.current_ma),
    powerMw: toNumberOrNull(rawHealth.power_mw),
    computedPowerMw: toNumberOrNull(rawHealth.computed_power_mw),
    audioQueueDepth: toNumberOrNull(rawHealth.audio_queue_depth),
    audioDroppedChunks: toNumberOrNull(rawHealth.audio_dropped_chunks),
  };
}

export function mapDeviceTimeSync(rawTimeSync) {
  if (!rawTimeSync) {
    return null;
  }

  return {
    id: Number(rawTimeSync.id),
    createdAt: nsToIso(rawTimeSync.created_at_ns),
    serverReceivedAt: nsToIso(rawTimeSync.server_received_epoch_ns),
    serverTransmitAt: nsToIso(rawTimeSync.server_transmit_epoch_ns),
    clientSentMonotonicNs: toNumberOrNull(rawTimeSync.client_sent_monotonic_ns),
  };
}

export function mapDevice(rawDevice) {
  return {
    id: String(rawDevice.id),
    lat: toCoordinate(rawDevice.lat),
    lon: toCoordinate(rawDevice.lon),
    name: rawDevice.name?.trim() || 'Unnamed device',
    tag: rawDevice.tag?.trim() || null,
    latestHealth: mapDeviceHealth(rawDevice.latest_health),
    latestTimeSync: mapDeviceTimeSync(rawDevice.latest_time_sync),
  };
}

export function serializeDevicePayload(input, options = {}) {
  const { includeId = false } = options;
  const payload = {
    lat: Number(input.lat),
    lon: Number(input.lon),
    name: input.name.trim(),
    tag: input.tag?.trim() || null,
  };

  if (includeId && input.id?.trim()) {
    payload.id = input.id.trim();
  }

  return payload;
}

export function parseCoordinates(value) {
  if (typeof value !== 'string') {
    return null;
  }

  const [latPart, lonPart] = value.split(',').map((item) => item.trim());
  const lat = Number(latPart);
  const lon = Number(lonPart);

  if (!Number.isFinite(lat) || !Number.isFinite(lon)) {
    return null;
  }

  return { lat, lon };
}

export function formatCoordinateInput(device) {
  if (!device || device.lat === null || device.lon === null) {
    return '';
  }

  return `${device.lat}, ${device.lon}`;
}
