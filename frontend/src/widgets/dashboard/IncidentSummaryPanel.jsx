import { AlertTriangle, MapPin, Radio } from 'lucide-react';
import { getIncidentTone } from '@/entities/incident/model/incident';
import { formatSensorCode } from '@/entities/sensor/model/sensor';
import { cx } from '@/shared/lib/cx';
import { formatCoordinate, formatDateTime, formatPercent } from '@/shared/lib/format';
import { Badge } from '@/shared/ui/Badge';
import { Panel } from '@/shared/ui/Panel';

export function IncidentSummaryPanel({
  activeIncident,
  className,
  focusIncident,
  onDismissActiveIncident,
}) {
  if (!focusIncident) {
    return (
      <Panel className={cx('incident-summary', className)}>
        <div className="panel-heading">
          <AlertTriangle size={16} />
          <span>Priority incident</span>
        </div>
        <p className="panel-empty">No active alert right now. Backend feed and simulation history are idle.</p>
      </Panel>
    );
  }

  const tone = getIncidentTone(focusIncident);

  return (
    <Panel className={cx('incident-summary', className)} tone={tone}>
      <div className="panel-heading">
        <AlertTriangle size={16} />
        <span>Priority incident</span>
      </div>

      <div className="incident-summary__headline">
        <div>
          <strong>{focusIncident.classification}</strong>
          <span>{formatDateTime(focusIncident.createdAt)}</span>
        </div>
        <Badge tone={tone === 'danger' ? 'danger' : 'warning'}>
          {focusIncident.source === 'simulation' ? 'Simulation' : 'Backend'}
        </Badge>
      </div>

      <p className="incident-summary__description">{focusIncident.description}</p>

      <div className="key-value-grid">
        <div className="key-value-row">
          <span>
            <MapPin size={14} />
            Coordinates
          </span>
          <strong>
            {formatCoordinate(focusIncident.lat)}, {formatCoordinate(focusIncident.lng)}
          </strong>
        </div>

        <div className="key-value-row">
          <span>
            <Radio size={14} />
            Confidence
          </span>
          <strong>{formatPercent(focusIncident.confidence)}</strong>
        </div>
      </div>

      {focusIncident.activeSensors?.length ? (
        <div className="incident-summary__sensors">
          {focusIncident.activeSensors.map((sensor) => (
            <Badge key={sensor.id} tone="neutral">
              {formatSensorCode(sensor)}
            </Badge>
          ))}
        </div>
      ) : null}

      {activeIncident ? (
        <button type="button" className="button-primary" onClick={onDismissActiveIncident}>
          Dismiss simulation alert
        </button>
      ) : null}
    </Panel>
  );
}
