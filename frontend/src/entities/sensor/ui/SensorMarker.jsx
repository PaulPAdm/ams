import { Mic } from 'lucide-react';
import { Marker } from 'react-map-gl';
import { cx } from '@/shared/lib/cx';
import { getSensorTone } from '@/entities/sensor/model/sensor';

export function SensorMarker({ sensor, isActive, isSelected, onSelect }) {
  const tone = isActive ? 'alert' : getSensorTone(sensor);

  return (
    <Marker latitude={sensor.lat} longitude={sensor.lng}>
      <button
        type="button"
        className={cx(
          'map-marker',
          'map-marker--sensor',
          `map-marker--${tone}`,
          isSelected && 'is-selected',
        )}
        onClick={(event) => {
          event.stopPropagation();
          onSelect(sensor);
        }}
        title={sensor.name}
      >
        <Mic size={isSelected ? 20 : 16} strokeWidth={2.3} />
        <span className="map-marker__halo" />
      </button>
    </Marker>
  );
}
