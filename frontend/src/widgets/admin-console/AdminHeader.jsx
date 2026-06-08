import { Activity, Database, HeartPulse, RefreshCw, ShieldCheck, Settings2 } from 'lucide-react';
import { MetricCard } from '@/shared/ui/MetricCard';
import { Panel } from '@/shared/ui/Panel';
import { Badge } from '@/shared/ui/Badge';
import { AppBrand } from '@/shared/ui/AppBrand';
import { cx } from '@/shared/lib/cx';
import { formatPower } from '@/shared/lib/format';

export function AdminHeader({
  avgPowerMw,
  className,
  deviceCount,
  incidentCount,
  isRefreshing,
  onCreateDevice,
  onOpenGlobalPeaks,
  onOpenHealth,
  onOpenOperations,
  onRefresh,
  reportingCount,
  systemHealth,
}) {
  const healthTone = systemHealth === 'Online' ? 'ok' : systemHealth === 'Offline' ? 'danger' : 'neutral';

  return (
    <Panel className={cx('admin-header', className)} tone="strong">
      <div className="admin-header__topline">
        <div className="admin-header__title-group">
          <AppBrand />
          <p>Devices, incidents and recordings in one place.</p>
        </div>

        <div className="admin-header__actions">
          <button type="button" className="button-secondary" onClick={onOpenOperations}>
            <ShieldCheck size={16} />
            Operations view
          </button>
          <button type="button" className="button-secondary" onClick={onOpenHealth}>
            <HeartPulse size={16} />
            Device health
          </button>
          <button type="button" className="button-secondary" onClick={onOpenGlobalPeaks}>
            <Activity size={16} />
            Global peaks
          </button>
          <button type="button" className="button-secondary" onClick={onRefresh}>
            <RefreshCw size={16} className={isRefreshing ? 'is-spinning' : ''} />
            Refresh
          </button>
          <button type="button" className="button-primary button-primary--inline" onClick={onCreateDevice}>
            <Settings2 size={16} />
            New device
          </button>
        </div>
      </div>

      <div className="admin-header__status">
        <Badge tone={healthTone}>
          <Database size={14} />
          System {systemHealth}
        </Badge>
      </div>

      <div className="metrics-grid">
        <MetricCard label="Registered devices" value={deviceCount} tone="accent" />
        <MetricCard label="Suspicious incidents" value={incidentCount} tone="warning" />
        <MetricCard
          label="Reporting devices"
          value={reportingCount}
          hint="Sent a health report"
          tone="accent"
        />
        <MetricCard
          label="Average power"
          value={Number.isFinite(avgPowerMw) ? formatPower(avgPowerMw) : '—'}
          hint="Across reporting devices"
          tone="default"
        />
      </div>
    </Panel>
  );
}
