import { AlertCircle, AlertTriangle } from 'lucide-react';
import { Marker } from 'react-map-gl';
import { cx } from '@/shared/lib/cx';
import { formatClock, formatCoordinate } from '@/shared/lib/format';

export function IncidentMarker({
  incident,
  variant = 'feed',
  isHovered = false,
  onHoverChange,
}) {
  if (variant === 'live') {
    return (
      <Marker latitude={incident.lat} longitude={incident.lng}>
        <div className="incident-anchor">
          <div className="map-marker map-marker--incident-live">
            <AlertTriangle size={22} strokeWidth={2.2} />
            <span className="map-marker__pulse" />
          </div>
        </div>
      </Marker>
    );
  }

  return (
    <Marker latitude={incident.lat} longitude={incident.lng}>
      <div
        className="incident-anchor"
        onMouseEnter={() => onHoverChange?.(incident.id)}
        onMouseLeave={() => onHoverChange?.(null)}
      >
        <div
          className={cx(
            'map-marker',
            'map-marker--incident-feed',
            isHovered && 'is-hovered',
          )}
        >
          <AlertCircle size={16} strokeWidth={2.2} />
          <span className="map-marker__radar" />
        </div>

        {isHovered ? (
          <div className="incident-tooltip">
            <div className="incident-tooltip__meta">
              <span>{incident.classification}</span>
              <span>{formatClock(incident.createdAt)}</span>
            </div>
            <p>{incident.description}</p>
            <div className="incident-tooltip__coords">
              <span>LAT {formatCoordinate(incident.lat)}</span>
              <span>LON {formatCoordinate(incident.lng)}</span>
            </div>
          </div>
        ) : null}
      </div>
    </Marker>
  );
}
