import { useMemo } from 'react';
import { Layer, Map, NavigationControl, Source } from 'react-map-gl';
import { appConfig, isMapConfigured } from '@/app/config/env';
import { buildTriangulationGeoJson } from '@/entities/incident/model/incident';
import { IncidentMarker } from '@/entities/incident/ui/IncidentMarker';
import { SensorMarker } from '@/entities/sensor/ui/SensorMarker';

const triangulationLayer = {
  id: 'triangulation-lines',
  type: 'line',
  paint: {
    'line-color': '#ff7a59',
    'line-width': 2,
    'line-opacity': 0.75,
    'line-dasharray': [1, 1.4],
  },
};

export function OperationsMap({
  activeIncident,
  hoveredIncidentId,
  onHoverIncident,
  onMapClick,
  onSelectSensor,
  onViewStateChange,
  selectedSensorId,
  suspiciousIncidents,
  sensors,
  viewState,
}) {
  const activeSensorIds = useMemo(
    () => new Set(activeIncident?.activeSensors?.map((sensor) => sensor.id) || []),
    [activeIncident],
  );

  const triangulationData = useMemo(
    () => buildTriangulationGeoJson(activeIncident),
    [activeIncident],
  );

  if (!isMapConfigured) {
    return (
      <div className="map-placeholder">
        <div className="map-placeholder__content">
          <span className="map-placeholder__eyebrow">Map disabled</span>
          <h2>Set <code>VITE_MAPBOX_TOKEN</code> to enable the operator map.</h2>
          <p>
            The rest of the console works without the basemap.
          </p>
        </div>
      </div>
    );
  }

  return (
    <Map
      {...viewState}
      mapStyle={appConfig.mapStyle}
      mapboxAccessToken={appConfig.mapboxToken}
      reuseMaps
      style={{ width: '100%', height: '100%' }}
      onClick={(event) => onMapClick(event.lngLat)}
      onMove={(event) => onViewStateChange(event.viewState)}
    >
      <NavigationControl position="bottom-right" />

      {triangulationData ? (
        <Source type="geojson" data={triangulationData}>
          <Layer {...triangulationLayer} />
        </Source>
      ) : null}

      {sensors.map((sensor) => (
        <SensorMarker
          key={sensor.id}
          sensor={sensor}
          isActive={activeSensorIds.has(sensor.id)}
          isSelected={selectedSensorId === sensor.id}
          onSelect={onSelectSensor}
        />
      ))}

      {suspiciousIncidents.map((incident) => (
        <IncidentMarker
          key={incident.id}
          incident={incident}
          isHovered={hoveredIncidentId === incident.id}
          onHoverChange={onHoverIncident}
        />
      ))}

      {activeIncident ? <IncidentMarker incident={activeIncident} variant="live" /> : null}
    </Map>
  );
}
