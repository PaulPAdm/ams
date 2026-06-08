import { appConfig } from '@/app/config/env';
import { deleteJson, getJson, postJson, putJson } from '@/shared/api/http';
import { mapDevice, mapDeviceHealth, serializeDevicePayload } from '@/entities/device/model/device';

export async function listDevices(options = {}) {
  const response = await getJson('/api/devices/', {
    params: {
      limit: appConfig.admin.limits.devices,
      skip: 0,
    },
    signal: options.signal,
  });

  return Array.isArray(response) ? response.map(mapDevice) : [];
}

export async function createDevice(input) {
  const response = await postJson(
    '/api/devices/',
    serializeDevicePayload(input, { includeId: true }),
  );

  return mapDevice(response);
}

export async function updateDevice(deviceId, input) {
  const response = await putJson(
    `/api/devices/${deviceId}`,
    serializeDevicePayload(input),
  );

  return mapDevice(response);
}

export function deleteDevice(deviceId) {
  return deleteJson(`/api/devices/${deviceId}`);
}

export async function listDeviceHealthReports(deviceId, { limit = 200, signal } = {}) {
  const response = await getJson(`/api/devices/${deviceId}/health`, {
    params: { limit, skip: 0 },
    signal,
  });

  const reports = Array.isArray(response) ? response.map(mapDeviceHealth).filter(Boolean) : [];
  // Backend returns newest-first; reverse to chronological order for charts.
  return reports.reverse();
}
