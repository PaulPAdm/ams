import { appConfig } from '@/app/config/env';
import { getJson } from '@/shared/api/http';
import { isSensorLocated, mapSensor } from '@/entities/sensor/model/sensor';

export async function listSensors(options = {}) {
  const response = await getJson('/api/locations/', {
    params: {
      limit: appConfig.limits.sensors,
    },
    signal: options.signal,
  });

  if (!Array.isArray(response)) {
    return [];
  }

  return response
    .map((sensor, index) => mapSensor(sensor, index))
    .filter(isSensorLocated);
}
