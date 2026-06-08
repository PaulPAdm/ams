import { AudioLines, Download, RefreshCw, Trash2, X } from 'lucide-react';
import { getAudioRecordDownloadUrl } from '@/entities/audio-record/api/audioRecordApi';
import { getAudioRecordFileName } from '@/entities/audio-record/model/audioRecord';
import { formatBytes, formatPreciseDateTime } from '@/shared/lib/format';

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
            <span>Acoustic events</span>
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
            {state.items.map((record) => {
              const downloadUrl = getAudioRecordDownloadUrl(record);
              const size = formatBytes(record.sizeBytes);

              return (
                <article key={record.id} className="drawer-list__item">
                  <div className="drawer-list__content">
                    <strong>{formatPreciseDateTime(record.eventTime || record.createdAt)}</strong>
                    <div className="drawer-list__meta">
                      <span>ID: {record.id}</span>
                      <span>{getAudioRecordFileName(record)}</span>
                      {record.contentType ? <span>{record.contentType}</span> : null}
                      {size ? <span>{size}</span> : null}
                    </div>
                    {downloadUrl ? (
                      <audio className="drawer-list__audio" controls preload="none" src={downloadUrl} />
                    ) : null}
                  </div>

                  <div className="admin-icon-actions">
                    {downloadUrl ? (
                      <a
                        className="icon-button"
                        href={downloadUrl}
                        download
                        title="Download record"
                      >
                        <Download size={16} />
                      </a>
                    ) : null}
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
              );
            })}
          </div>
        ) : (
          <p className="panel-empty">No acoustic events available.</p>
        )}
      </div>
    </aside>
  );
}
