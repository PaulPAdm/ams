import { Activity, Battery, MapPin, Radio } from 'lucide-react';
import { formatSensorCode, getSensorTone } from '@/entities/sensor/model/sensor';
import { cx } from '@/shared/lib/cx';
import { formatCoordinate, formatDb, formatPercent } from '@/shared/lib/format';
import { Badge } from '@/shared/ui/Badge';
import { Panel } from '@/shared/ui/Panel';

export function SensorDetailsPanel({ className, liveAudioLevel, selectedSensor }) {
  if (!selectedSensor) {
    return (
      <Panel className={cx('sensor-details', className)}>
        <div className="panel-heading">
          <Radio size={16} />
          <span>Sensor telemetry</span>
        </div>
        <p className="panel-empty">
          Select a node on the map to inspect live telemetry, status, and coordinates.
        </p>
      </Panel>
    );
  }

  const tone = getSensorTone(selectedSensor);

  return (
    <Panel className={cx('sensor-details', className)} tone={tone === 'warning' ? 'warning' : 'default'}>
      <div className="panel-heading">
        <Radio size={16} />
        <span>Sensor telemetry</span>
      </div>

      <div className="sensor-details__title">
        <div>
          <strong>{selectedSensor.name}</strong>
          <span>{formatSensorCode(selectedSensor)}</span>
        </div>
        <Badge tone={tone === 'warning' ? 'warning' : 'ok'}>{selectedSensor.status}</Badge>
      </div>

      <div className="key-value-grid">
        <div className="key-value-row">
          <span>
            <MapPin size={14} />
            Coordinates
          </span>
          <strong>
            {formatCoordinate(selectedSensor.lat)}, {formatCoordinate(selectedSensor.lng)}
          </strong>
        </div>
        <div className="key-value-row">
          <span>
            <Battery size={14} />
            Battery
          </span>
          <strong>{formatPercent(selectedSensor.battery)}</strong>
        </div>
        <div className="sensor-meter">
          <div className="sensor-meter__track">
            <div
              className="sensor-meter__fill"
              style={{ width: `${selectedSensor.battery}%` }}
            />
          </div>
        </div>
        <div className="key-value-row">
          <span>
            <Activity size={14} />
            Live audio level
          </span>
          <strong>{formatDb(liveAudioLevel)}</strong>
        </div>
      </div>
    </Panel>
  );
}
