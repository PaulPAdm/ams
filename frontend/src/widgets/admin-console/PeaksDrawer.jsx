import { Activity, RefreshCw, Trash2, X } from 'lucide-react';
import { formatPreciseDateTime } from '@/shared/lib/format';

export function PeaksDrawer({ state, onClear, onClose, onDelete, onRefresh }) {
  if (!state.isOpen) {
    return null;
  }

  return (
    <aside className="side-drawer">
      <div className="side-drawer__header">
        <div>
          <div className="panel-heading">
            <Activity size={16} />
            <span>{state.isGlobal ? 'Global peaks' : 'Device peaks'}</span>
          </div>
          <p className="side-drawer__subtitle">
            {state.isGlobal ? 'All sound peaks from the backend.' : `Device: ${state.deviceId}`}
          </p>
        </div>

        <div className="side-drawer__actions">
          <button type="button" className="icon-button" onClick={onRefresh} title="Refresh peaks">
            <RefreshCw size={16} className={state.isLoading ? 'is-spinning' : ''} />
          </button>
          {state.isGlobal ? (
            <button type="button" className="icon-button icon-button--danger" onClick={onClear} title="Delete all peaks">
              <Trash2 size={16} />
            </button>
          ) : null}
          <button type="button" className="icon-button" onClick={onClose} title="Close drawer">
            <X size={16} />
          </button>
        </div>
      </div>

      {state.error ? <div className="admin-error-banner">{state.error.message || 'Failed to load peaks.'}</div> : null}

      <div className="side-drawer__content">
        {state.items.length ? (
          <div className="drawer-list">
            {state.items.map((peak) => (
              <article key={peak.id} className="drawer-list__item">
                <div>
                  <strong>{formatPreciseDateTime(peak.peakTime)}</strong>
                  <div className="drawer-list__meta">
                    <span>ID: {peak.id}</span>
                    <span>Received: {formatPreciseDateTime(peak.receivedAt)}</span>
                    {state.isGlobal ? <span>Device: {peak.deviceId}</span> : null}
                  </div>
                </div>

                <button
                  type="button"
                  className="icon-button icon-button--danger"
                  onClick={() => onDelete(peak.id)}
                  title="Delete peak"
                >
                  <Trash2 size={16} />
                </button>
              </article>
            ))}
          </div>
        ) : (
          <p className="panel-empty">No peaks available.</p>
        )}
      </div>
    </aside>
  );
}
