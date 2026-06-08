import { buildApiUrl, getJson, postFormData } from '@/shared/api/http';
import { mapSoundEvent } from '@/entities/sound-event/model/soundEvent';

export async function listDeviceSoundEvents(deviceId, options = {}) {
  const response = await getJson(`/api/devices/${deviceId}/sound-events`, {
    params: {
      limit: options.limit ?? 20,
      skip: 0,
    },
    signal: options.signal,
  });

  return Array.isArray(response) ? response.map(mapSoundEvent) : [];
}

export function getSoundEventAudioUrl(event) {
  return event.audioDownloadUrl ? buildApiUrl(event.audioDownloadUrl) : null;
}

export async function uploadSoundEventAudioFile(deviceId, eventId, file, options = {}) {
  const body = new FormData();
  body.append('file', file);

  const response = await postFormData(`/api/devices/${deviceId}/sound-events/${eventId}/audio`, body, {
    signal: options.signal,
  });

  return mapSoundEvent(response);
}
