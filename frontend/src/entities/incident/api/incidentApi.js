import { appConfig } from '@/app/config/env';
import { deleteJson, getJson } from '@/shared/api/http';
import { isIncidentLocated, mapIncident } from '@/entities/incident/model/incident';

export async function listIncidents(options = {}) {
  const response = await getJson('/api/suspicious_incidents/', {
    params: {
      limit: appConfig.limits.incidents,
    },
    signal: options.signal,
  });

  if (!Array.isArray(response)) {
    return [];
  }

  const incidents = response.map(mapIncident);
  return options.includeUnlocated ? incidents : incidents.filter(isIncidentLocated);
}

export function deleteIncident(incidentId) {
  return deleteJson(`/api/suspicious_incidents/${incidentId}`);
}

export function clearIncidents() {
  return deleteJson('/api/suspicious_incidents/');
}
