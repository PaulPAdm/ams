import { appConfig } from '@/app/config/env';
import { buildApiUrl, deleteJson, getJson } from '@/shared/api/http';
import { mapAudioRecord } from '@/entities/audio-record/model/audioRecord';

export async function listDeviceAudioRecords(deviceId) {
  const response = await getJson(`/api/devices/${deviceId}/audio_records`, {
    params: {
      limit: appConfig.admin.limits.audioRecords,
      skip: 0,
    },
  });

  return Array.isArray(response) ? response.map(mapAudioRecord) : [];
}

export function deleteAudioRecord(recordId) {
  return deleteJson(`/api/audio_records/${recordId}`);
}

export function clearDeviceAudioRecords(deviceId) {
  return deleteJson(`/api/devices/${deviceId}/audio_records`);
}

export function getAudioRecordDownloadUrl(record) {
  return buildApiUrl(record.downloadUrl || `/api/audio_records/${record.id}/download`);
}
