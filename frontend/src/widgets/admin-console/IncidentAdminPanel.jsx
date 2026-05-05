import { RefreshCw, Target, Trash2 } from 'lucide-react';
import { Panel } from '@/shared/ui/Panel';
import { cx } from '@/shared/lib/cx';
import { formatCoordinate, formatPreciseDateTime } from '@/shared/lib/format';

export function IncidentAdminPanel({
  className,
  error,
  incidents,
  onClear,
  onDelete,
  onRefresh,
}) {
  return (
    <Panel className={cx('admin-panel', className)}>
      <div className="admin-panel__header">
        <div className="panel-heading">
          <Target size={16} />
          <span>Suspicious incidents</span>
        </div>

        <div className="admin-panel__actions">
          <button type="button" className="icon-button" onClick={onRefresh} title="Refresh incidents">
            <RefreshCw size={14} />
          </button>
          <button
            type="button"
            className="icon-button icon-button--danger"
            onClick={onClear}
            title="Delete all incidents"
          >
            <Trash2 size={14} />
          </button>
        </div>
      </div>

      {error ? (
        <div className="admin-error-banner">
          <span>{error.message || 'Failed to load suspicious incidents.'}</span>
        </div>
      ) : null}

      {incidents.length ? (
        <div className="admin-table-wrap">
          <table className="admin-table">
            <thead>
              <tr>
                <th>ID</th>
                <th>Detected at</th>
                <th>Coordinates</th>
                <th>Description</th>
                <th className="align-right">Action</th>
              </tr>
            </thead>
            <tbody>
              {incidents.map((incident) => (
                <tr key={incident.id}>
                  <td>
                    <code className="admin-code">#{incident.id}</code>
                  </td>
                  <td>{formatPreciseDateTime(incident.createdAt)}</td>
                  <td className="admin-table__coords">
                    {incident.lat !== null && incident.lng !== null
                      ? `${formatCoordinate(incident.lat)}, ${formatCoordinate(incident.lng)}`
                      : 'n/a'}
                  </td>
                  <td>{incident.description || 'No description'}</td>
                  <td className="align-right">
                    <button
                      type="button"
                      className="icon-button icon-button--danger"
                      onClick={() => onDelete(incident.id)}
                      title="Delete incident"
                    >
                      <Trash2 size={16} />
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (
        <p className="panel-empty">No suspicious incidents available.</p>
      )}
    </Panel>
  );
}
