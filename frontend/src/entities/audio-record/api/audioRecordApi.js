import { appConfig } from '@/app/config/env';
import { buildApiUrl, deleteJson, getJson } from '@/shared/api/http';
import { mapAudioRecord } from '@/entities/audio-record/model/audioRecord';

export async function listDeviceAudioRecords(deviceId) {
  const response = await getJson(`/api/devices/${deviceId}/sound-events`, {
    params: {
      limit: appConfig.admin.limits.audioRecords,
      skip: 0,
    },
  });

  return Array.isArray(response) ? response.map(mapAudioRecord) : [];
}

export function deleteAudioRecord(deviceId, recordId) {
  return deleteJson(`/api/devices/${deviceId}/sound-events/${recordId}`);
}

export function clearDeviceAudioRecords(deviceId) {
  return deleteJson(`/api/devices/${deviceId}/sound-events`);
}

export function getAudioRecordDownloadUrl(record) {
  return record.downloadUrl ? buildApiUrl(record.downloadUrl) : null;
}
