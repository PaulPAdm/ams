import { Activity, Cpu, Database, RefreshCw, ShieldCheck, Settings2 } from 'lucide-react';
import { MetricCard } from '@/shared/ui/MetricCard';
import { Panel } from '@/shared/ui/Panel';
import { Badge } from '@/shared/ui/Badge';
import { cx } from '@/shared/lib/cx';

export function AdminHeader({
  className,
  deviceCount,
  incidentCount,
  isRefreshing,
  onCreateDevice,
  onOpenGlobalPeaks,
  onOpenOperations,
  onRefresh,
  systemHealth,
}) {
  const healthTone = systemHealth === 'Online' ? 'ok' : systemHealth === 'Offline' ? 'danger' : 'neutral';

  return (
    <Panel className={cx('admin-header', className)} tone="strong">
      <div className="admin-header__topline">
        <div className="admin-header__title-group">
          <div className="dashboard-header__eyebrow">Integrated admin console</div>
          <h1>Registry, incident operations, and record cleanup</h1>
          <p>
            Device provisioning, suspicious incident review, peak cleanup, and audio archive access
            now live inside the main frontend.
          </p>
        </div>

        <div className="admin-header__actions">
          <button type="button" className="button-secondary" onClick={onOpenOperations}>
            <ShieldCheck size={16} />
            Operations view
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
        <Badge tone="neutral">
          <Cpu size={14} />
          Unified frontend deployment
        </Badge>
      </div>

      <div className="metrics-grid">
        <MetricCard
          label="Registered devices"
          value={deviceCount}
          hint="FastAPI location registry"
          tone="accent"
        />
        <MetricCard
          label="Suspicious incidents"
          value={incidentCount}
          hint="Admin review queue"
          tone="warning"
        />
        <MetricCard
          label="Peak tools"
          value="Ready"
          hint="Global and per-device inspection"
          tone="accent"
        />
        <MetricCard
          label="Audio cleanup"
          value="Ready"
          hint="Download and delete recordings"
          tone="default"
        />
      </div>
    </Panel>
  );
}
