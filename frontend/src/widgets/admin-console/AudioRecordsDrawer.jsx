import { AudioLines, Download, RefreshCw, Trash2, X } from 'lucide-react';
import { getAudioRecordDownloadUrl } from '@/entities/audio-record/api/audioRecordApi';
import { getAudioRecordFileName } from '@/entities/audio-record/model/audioRecord';
import { formatPreciseDateTime } from '@/shared/lib/format';

export function AudioRecordsDrawer({ state, onClear, onClose, onDelete, onRefresh }) {
  if (!state.isOpen) {
    return null;
  }

  return (
    <aside className="side-drawer">
      <div className="side-drawer__header">
        <div>
          <div className="panel-heading">
            <AudioLines size={16} />
            <span>Audio records</span>
          </div>
          <p className="side-drawer__subtitle">Device: {state.deviceId}</p>
        </div>

        <div className="side-drawer__actions">
          <button type="button" className="icon-button" onClick={onRefresh} title="Refresh records">
            <RefreshCw size={16} className={state.isLoading ? 'is-spinning' : ''} />
          </button>
          <button type="button" className="icon-button icon-button--danger" onClick={onClear} title="Delete all records">
            <Trash2 size={16} />
          </button>
          <button type="button" className="icon-button" onClick={onClose} title="Close drawer">
            <X size={16} />
          </button>
        </div>
      </div>

      {state.error ? <div className="admin-error-banner">{state.error.message || 'Failed to load audio records.'}</div> : null}

      <div className="side-drawer__content">
        {state.items.length ? (
          <div className="drawer-list">
            {state.items.map((record) => (
              <article key={record.id} className="drawer-list__item">
                <div>
                  <strong>{formatPreciseDateTime(record.createdAt)}</strong>
                  <div className="drawer-list__meta">
                    <span>ID: {record.id}</span>
                    <span>{getAudioRecordFileName(record)}</span>
                  </div>
                </div>

                <div className="admin-icon-actions">
                  <a
                    className="icon-button"
                    href={getAudioRecordDownloadUrl(record)}
                    download
                    title="Download record"
                  >
                    <Download size={16} />
                  </a>
                  <button
                    type="button"
                    className="icon-button icon-button--danger"
                    onClick={() => onDelete(record.id)}
                    title="Delete record"
                  >
                    <Trash2 size={16} />
                  </button>
                </div>
              </article>
            ))}
          </div>
        ) : (
          <p className="panel-empty">No audio records available.</p>
        )}
      </div>
    </aside>
  );
}
