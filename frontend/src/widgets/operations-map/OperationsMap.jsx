import { Map, NavigationControl } from 'react-map-gl';
import mapboxgl from 'mapbox-gl';
import { appConfig, isMapConfigured } from '@/app/config/env';
import { IncidentMarker } from '@/entities/incident/ui/IncidentMarker';
import { IncidentPopup } from '@/entities/incident/ui/IncidentPopup';
import { SensorMarker } from '@/entities/sensor/ui/SensorMarker';

const isMapSupported = typeof window !== 'undefined' && mapboxgl.supported();

function MapPlaceholder({ eyebrow, title, message }) {
  return (
    <div className="map-placeholder">
      <div className="map-placeholder__content">
        <span className="map-placeholder__eyebrow">{eyebrow}</span>
        <h2>{title}</h2>
        <p>{message}</p>
      </div>
    </div>
  );
}

export function OperationsMap({
  incidents,
  onClearSelection,
  onHideIncident,
  onSelectIncident,
  onSelectSensor,
  onViewStateChange,
  selectedIncident,
  selectedSensor,
  sensors,
  viewState,
}) {
  if (!isMapConfigured) {
    return (
      <MapPlaceholder
        eyebrow="Map disabled"
        title="Map configuration is missing."
        message="The rest of the console works without the basemap."
      />
    );
  }

  if (!isMapSupported) {
    return (
      <MapPlaceholder
        eyebrow="Map unavailable"
        title="This browser cannot render the operator map."
        message="Sensor and incident data remain available in the side panels."
      />
    );
  }

  return (
    <Map
      {...viewState}
      mapStyle={appConfig.mapStyle}
      mapboxAccessToken={appConfig.mapboxToken}
      reuseMaps
      style={{ width: '100%', height: '100%' }}
      onClick={onClearSelection}
      onMove={(event) => onViewStateChange(event.viewState)}
    >
      <NavigationControl position="bottom-right" />

      {sensors.map((sensor) => (
        <SensorMarker
          key={sensor.id}
          sensor={sensor}
          isActive={selectedSensor?.id === sensor.id}
          onSelect={onSelectSensor}
        />
      ))}

      {incidents.map((incident) => (
        <IncidentMarker
          key={incident.id}
          incident={incident}
          isSelected={selectedIncident?.id === incident.id}
          onSelect={onSelectIncident}
        />
      ))}

      <IncidentPopup incident={selectedIncident} onClose={onClearSelection} onHide={onHideIncident} />
    </Map>
  );
}
