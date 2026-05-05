import * as turf from '@turf/turf';

const simulationLabels = [
  'Scream cluster',
  'Glass impact',
  'Gunshot signature',
  'Pressure blast',
];

const randomBetween = (min, max) => {
  if (max <= min) {
    return min;
  }

  return Math.round(Math.random() * (max - min) + min);
};

const createIncidentId = () => {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID();
  }

  return `incident-${Date.now()}`;
};

export function mapIncident(rawIncident) {
  const lat = Number(rawIncident.lat);
  const lng = Number(rawIncident.lon ?? rawIncident.lng);

  return {
    id: String(rawIncident.id),
    lat,
    lng,
    description: rawIncident.description?.trim() || 'Suspicious activity flagged by backend.',
    createdAt: rawIncident.created_at || new Date().toISOString(),
    classification: 'Suspicious incident',
    confidence: null,
    source: 'backend',
    activeSensors: [],
  };
}

export function isIncidentLocated(incident) {
  return Number.isFinite(incident.lat) && Number.isFinite(incident.lng);
}

export function buildSimulationIncident(options) {
  const {
    lat,
    lng,
    activeSensors,
    minConfidence,
    maxConfidence,
  } = options;
  const classification = simulationLabels[Math.floor(Math.random() * simulationLabels.length)];
  const confidence = randomBetween(minConfidence, maxConfidence);

  return {
    id: createIncidentId(),
    lat,
    lng,
    description: `${classification} matched across ${activeSensors.length} nearest nodes.`,
    createdAt: new Date().toISOString(),
    classification,
    confidence,
    source: 'simulation',
    activeSensors,
  };
}

export function buildTriangulationGeoJson(incident) {
  if (!incident?.activeSensors?.length) {
    return null;
  }

  return turf.featureCollection(
    incident.activeSensors.map((sensor) =>
      turf.lineString([
        [incident.lng, incident.lat],
        [sensor.lng, sensor.lat],
      ]),
    ),
  );
}

export function getIncidentTone(incident) {
  if (!incident) {
    return 'default';
  }

  return incident.source === 'simulation' ? 'danger' : 'warning';
}
