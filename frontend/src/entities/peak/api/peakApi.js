import { appConfig } from '@/app/config/env';
import { deleteJson, getJson } from '@/shared/api/http';
import { mapPeak } from '@/entities/peak/model/peak';

function mapPeakCollection(response) {
  return Array.isArray(response) ? response.map(mapPeak) : [];
}

export async function listPeaks() {
  const response = await getJson('/api/peaks/', {
    params: {
      limit: appConfig.admin.limits.peaks,
      skip: 0,
    },
  });

  return mapPeakCollection(response);
}

export async function listDevicePeaks(deviceId) {
  const response = await getJson(`/api/devices/${deviceId}/peaks`, {
    params: {
      limit: appConfig.admin.limits.peaks,
      skip: 0,
    },
  });

  return mapPeakCollection(response);
}

export function deletePeak(peakId) {
  return deleteJson(`/api/peaks/${peakId}`);
}

export function clearPeaks() {
  return deleteJson('/api/peaks/');
}
