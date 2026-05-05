import { Activity, Globe, Radio, ShieldAlert } from 'lucide-react';
import { formatSensorCode } from '@/entities/sensor/model/sensor';
import { cx } from '@/shared/lib/cx';
import { formatDateTime } from '@/shared/lib/format';
import { Badge } from '@/shared/ui/Badge';
import { Panel } from '@/shared/ui/Panel';

export function SystemHealthPanel({
  className,
  incidentsError,
  incidentsLoading,
  lastUpdated,
  selectedSensor,
  sensorsError,
  sensorsLoading,
  sensorSource,
}) {
  const incidentsTone = incidentsError ? 'danger' : incidentsLoading ? 'neutral' : 'ok';
  const sensorsTone = sensorsError ? 'warning' : sensorsLoading ? 'neutral' : 'ok';

  return (
    <Panel className={cx('system-health', className)}>
      <div className="panel-heading">
        <ShieldAlert size={16} />
        <span>System health</span>
      </div>

      <div className="status-list">
        <div className="status-row">
          <span>
            <Radio size={14} />
            Device registry
          </span>
          <Badge tone={sensorsTone}>
            {sensorsError ? 'Fallback' : sensorsLoading ? 'Loading' : sensorSource === 'remote' ? 'Live' : 'Cached'}
          </Badge>
        </div>

        <div className="status-row">
          <span>
            <Activity size={14} />
            Incident polling
          </span>
          <Badge tone={incidentsTone}>
            {incidentsError ? 'Degraded' : incidentsLoading ? 'Syncing' : 'Live'}
          </Badge>
        </div>

        <div className="status-row">
          <span>
            <Globe size={14} />
            Last sync
          </span>
          <strong>{formatDateTime(lastUpdated)}</strong>
        </div>

        <div className="status-row">
          <span>Selected node</span>
          <strong>{selectedSensor ? formatSensorCode(selectedSensor) : 'None'}</strong>
        </div>
      </div>
    </Panel>
  );
}
