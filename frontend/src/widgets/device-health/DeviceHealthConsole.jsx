import { ArrowLeft, RefreshCw } from 'lucide-react';
import { AppBrand } from '@/shared/ui/AppBrand';
import { MetricCard } from '@/shared/ui/MetricCard';
import { Panel } from '@/shared/ui/Panel';
import { formatCurrent, formatPower, formatPreciseDateTime, formatVoltage } from '@/shared/lib/format';
import { useDeviceHealth } from '@/features/device-health/model/useDeviceHealth';
import { AudioPipelineChart } from '@/widgets/device-health/charts/AudioPipelineChart';
import { PowerChart } from '@/widgets/device-health/charts/PowerChart';

const formatCount = (value) => (Number.isFinite(value) ? String(value) : '—');

export function DeviceHealthConsole({ onOpenAdmin }) {
  const {
    devices,
    devicesError,
    error,
    isLoading,
    refresh,
    reports,
    selectDevice,
    selectedDeviceId,
    summary,
  } = useDeviceHealth();

  const latest = summary.latest;
  const hasData = reports.length > 0;
  const hasDevices = devices.length > 0;
  const chartOverlay = isLoading
    ? 'Loading…'
    : !hasDevices
      ? 'No devices available'
      : 'No health reports for this device yet';

  return (
    <div className="admin-console">
      <div className="admin-console__layout">
        <Panel className="admin-header health-console__header" tone="strong">
          <div className="admin-header__topline">
            <div className="admin-header__title-group">
              <AppBrand />
              <p>Device health — power and audio pipeline telemetry over time.</p>
            </div>

            <div className="admin-header__actions">
              <button type="button" className="button-secondary" onClick={onOpenAdmin}>
                <ArrowLeft size={16} />
                Back to admin
              </button>
              <button type="button" className="button-secondary" onClick={refresh}>
                <RefreshCw size={16} className={isLoading ? 'is-spinning' : ''} />
                Refresh
              </button>
            </div>
          </div>

          <div className="health-console__controls">
            <label className="health-console__select">
              <span>Device</span>
              <select
                value={selectedDeviceId || ''}
                onChange={(event) => selectDevice(event.target.value)}
                disabled={!hasDevices}
              >
                {!hasDevices ? <option value="">No devices</option> : null}
                {devices.map((device) => (
                  <option key={device.id} value={device.id}>
                    {device.name} ({device.id})
                  </option>
                ))}
              </select>
            </label>
            {latest ? (
              <span className="health-console__updated">
                Latest report: {formatPreciseDateTime(latest.receivedAt)}
              </span>
            ) : null}
          </div>

          <div className="metrics-grid">
            <MetricCard label="Bus voltage" value={latest ? formatVoltage(latest.busVoltageV) : '—'} tone="accent" />
            <MetricCard label="Current" value={latest ? formatCurrent(latest.currentMa) : '—'} tone="accent" />
            <MetricCard label="Power" value={latest ? formatPower(latest.powerMw) : '—'} tone="default" />
            <MetricCard label="Queue depth" value={latest ? formatCount(latest.audioQueueDepth) : '—'} tone="warning" />
            <MetricCard label="Dropped chunks" value={formatCount(summary.droppedTotal)} tone="danger" />
          </div>
        </Panel>

        {devicesError ? (
          <div className="admin-error-banner">
            <span>Could not load devices — is the backend running? ({devicesError.message || 'request failed'})</span>
          </div>
        ) : null}

        {error ? (
          <div className="admin-error-banner">
            <span>{error.message || 'Failed to load device health.'}</span>
          </div>
        ) : null}

        <Panel className="chart-panel">
          <div className="panel-heading">
            <span>Power</span>
          </div>
          <div className="chart-panel__canvas">
            <PowerChart reports={reports} />
            {!hasData ? <div className="chart-panel__overlay">{chartOverlay}</div> : null}
          </div>
        </Panel>

        <Panel className="chart-panel">
          <div className="panel-heading">
            <span>Audio pipeline</span>
          </div>
          <div className="chart-panel__canvas">
            <AudioPipelineChart reports={reports} />
            {!hasData ? <div className="chart-panel__overlay">{chartOverlay}</div> : null}
          </div>
        </Panel>
      </div>
    </div>
  );
}
