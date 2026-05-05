const toCoordinate = (value) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
};

export function mapDevice(rawDevice) {
  return {
    id: String(rawDevice.id),
    lat: toCoordinate(rawDevice.lat),
    lon: toCoordinate(rawDevice.lon),
    name: rawDevice.name?.trim() || 'Unnamed device',
    tag: rawDevice.tag?.trim() || null,
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
