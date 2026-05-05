import { Activity, Globe, MapPin, Radio } from 'lucide-react';
import { appConfig } from '@/app/config/env';
import { cx } from '@/shared/lib/cx';
import { Badge } from '@/shared/ui/Badge';
import { MetricCard } from '@/shared/ui/MetricCard';
import { Panel } from '@/shared/ui/Panel';

export function DashboardHeader({
  appTitle,
  className,
  incidentCount,
  mapConfigured,
  onOpenAdmin,
  onToggleSimulation,
  sensorCount,
  sensorSource,
  simulationEnabled,
  zoomLevel,
}) {
  return (
    <Panel className={cx('dashboard-header', className)} tone="strong">
      <div className="dashboard-header__eyebrow">Operational acoustic mesh</div>

      <div className="dashboard-header__topline">
        <div className="dashboard-header__title-group">
          <h1>{appTitle}</h1>
          <p>
            Live city map, suspicious incident feed, and fast simulation mode for triangulation
            checks.
          </p>
        </div>

        <div className="dashboard-header__actions">
          <button
            type="button"
            className="button-secondary"
            onClick={onOpenAdmin}
          >
            <Globe size={16} />
            Admin console
          </button>

          <button
            type="button"
            className={cx('toggle-chip', simulationEnabled && 'is-active')}
            onClick={onToggleSimulation}
          >
            <span className="toggle-chip__dot" />
            Simulation {simulationEnabled ? 'On' : 'Off'}
          </button>
        </div>
      </div>

      <div className="dashboard-header__badges">
        <Badge tone={mapConfigured ? 'ok' : 'warning'}>
          <MapPin size={14} />
          {mapConfigured ? 'Map online' : 'Map token missing'}
        </Badge>
        <Badge tone={sensorSource === 'remote' ? 'ok' : 'warning'}>
          <Radio size={14} />
          {sensorSource === 'remote' ? 'Live registry' : 'Fallback nodes'}
        </Badge>
        <Badge tone="neutral">
          <Globe size={14} />
          {appConfig.cityLabel}
        </Badge>
      </div>

      <div className="metrics-grid">
        <MetricCard
          label="Deployed nodes"
          value={sensorCount}
          hint="Active acoustic endpoints"
          tone="accent"
        />
        <MetricCard
          label="Incident feed"
          value={incidentCount}
          hint="Backend + simulated alerts"
          tone="warning"
        />
        <MetricCard
          label="Map zoom"
          value={`${zoomLevel.toFixed(1)}x`}
          hint="Current operator scale"
          tone="accent"
        />
        <MetricCard
          label="Simulation"
          value={simulationEnabled ? 'Armed' : 'Standby'}
          hint="Tap the map to inject a test event"
          tone={simulationEnabled ? 'danger' : 'default'}
        />
      </div>

      <div className="dashboard-header__footnote">
        <Activity size={14} />
        <span>Single-page operator console rebuilt into app / shared / entities / features / widgets.</span>
      </div>
    </Panel>
  );
}
