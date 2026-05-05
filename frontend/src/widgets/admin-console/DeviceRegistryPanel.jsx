import { Activity, AudioLines, Cpu, Edit, RefreshCw, Trash2 } from 'lucide-react';
import { Panel } from '@/shared/ui/Panel';
import { Badge } from '@/shared/ui/Badge';
import { cx } from '@/shared/lib/cx';
import { formatCoordinate } from '@/shared/lib/format';

export function DeviceRegistryPanel({
  className,
  devices,
  error,
  onDelete,
  onEdit,
  onInit,
  onOpenAudio,
  onOpenPeaks,
}) {
  return (
    <Panel className={cx('admin-panel', className)}>
      <div className="panel-heading">
        <Cpu size={16} />
        <span>Device registry</span>
      </div>

      {error ? (
        <div className="admin-error-banner">
          <span>{error.message || 'Failed to load devices.'}</span>
        </div>
      ) : null}

      <div className="admin-table-wrap">
        <table className="admin-table">
          <thead>
            <tr>
              <th>ID</th>
              <th>Name</th>
              <th>Tag</th>
              <th>Coordinates</th>
              <th className="align-right">Actions</th>
            </tr>
          </thead>
          <tbody>
            {devices.map((device) => (
              <tr key={device.id}>
                <td>
                  <code className="admin-code admin-code--compact">{device.id}</code>
                </td>
                <td>{device.name}</td>
                <td>
                  {device.tag ? <Badge tone="neutral">{device.tag}</Badge> : <span className="muted-copy">None</span>}
                </td>
                <td className="admin-table__coords">
                  {device.lat !== null && device.lon !== null
                    ? `${formatCoordinate(device.lat)}, ${formatCoordinate(device.lon)}`
                    : 'n/a'}
                </td>
                <td className="align-right">
                  <div className="admin-icon-actions">
                    <button type="button" className="icon-button" title="Initialize device" onClick={() => onInit(device.id)}>
                      <RefreshCw size={16} />
                    </button>
                    <button type="button" className="icon-button" title="Open audio records" onClick={() => onOpenAudio(device.id)}>
                      <AudioLines size={16} />
                    </button>
                    <button type="button" className="icon-button" title="Open peaks" onClick={() => onOpenPeaks(device.id)}>
                      <Activity size={16} />
                    </button>
                    <button type="button" className="icon-button" title="Edit device" onClick={() => onEdit(device)}>
                      <Edit size={16} />
                    </button>
                    <button
                      type="button"
                      className="icon-button icon-button--danger"
                      title="Delete device"
                      onClick={() => onDelete(device.id)}
                    >
                      <Trash2 size={16} />
                    </button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {!devices.length ? <p className="panel-empty">No devices registered yet.</p> : null}
    </Panel>
  );
}
