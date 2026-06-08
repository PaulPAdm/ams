import { Battery, Download, MapPin, Mic, RefreshCw, Tag, Upload, Volume2, X } from 'lucide-react';
import { getSensorLocationSummary } from '@/entities/sensor/model/sensor';
import { getSoundEventAudioUrl } from '@/entities/sound-event/api/soundEventApi';
import {
  formatBytes,
  formatCoordinate,
  formatCurrent,
  formatPower,
  formatPreciseDateTime,
  formatVoltage,
} from '@/shared/lib/format';

const AUDIO_FILE_ACCEPT = [
  'audio/wav',
  'audio/mpeg',
  'audio/ogg',
  'audio/flac',
  'audio/mp4',
  'audio/aac',
  'audio/webm',
  '.wav',
  '.mp3',
  '.ogg',
  '.oga',
  '.flac',
  '.m4a',
  '.mp4',
  '.aac',
  '.webm',
].join(',');

export function SensorDetailsDrawer({
  sensor,
  soundEventsState = { error: null, isLoading: false, items: [] },
  onClose,
  onRefreshSoundEvents,
  onUploadSoundEventAudio,
}) {
  if (!sensor) {
    return null;
  }

  const coordinates = `${formatCoordinate(sensor.lat)}, ${formatCoordinate(sensor.lng)}`;
  const latestHealth = sensor.latestHealth;
  const latestTimeSync = sensor.latestTimeSync;
  const powerUnknown =
    latestHealth?.ina219Online === null &&
    latestHealth?.busVoltageV === null &&
    latestHealth?.currentMa === null &&
    latestHealth?.powerMw === null;
  const handleAudioUploadChange = (soundEvent, changeEvent) => {
    const file = changeEvent.target.files?.[0];
    changeEvent.target.value = '';

    if (!file || !onUploadSoundEventAudio) {
      return;
    }

    onUploadSoundEventAudio(soundEvent.id, file);
  };

  return (
    <aside className="sensor-details-drawer" aria-label="Device details">
      <div className="sensor-details-drawer__header">
        <div>
          <div className="panel-heading">
            <Mic size={16} />
            <span>Device details</span>
          </div>
          <h2>{sensor.name}</h2>
          <p>{getSensorLocationSummary(sensor)}</p>
        </div>

        <button type="button" className="icon-button" onClick={onClose} title="Close device panel">
          <X size={16} />
        </button>
      </div>

      <dl className="sensor-details-list">
        <div>
          <dt>
            <Mic size={15} />
            <span>Device ID</span>
          </dt>
          <dd><code>{sensor.id}</code></dd>
        </div>

        <div>
          <dt>
            <Tag size={15} />
            <span>Tag</span>
          </dt>
          <dd>{sensor.tag || 'Not assigned'}</dd>
        </div>

        <div>
          <dt>
            <MapPin size={15} />
            <span>Location</span>
          </dt>
          <dd>{coordinates}</dd>
        </div>

        <div>
          <dt>
            <Battery size={15} />
            <span>Battery state</span>
          </dt>
          <dd>
            {latestHealth ? (
              <>
                {powerUnknown
                  ? 'Power: Unknown'
                  : `${formatVoltage(latestHealth.busVoltageV)} / ${formatCurrent(latestHealth.currentMa)} / ${formatPower(latestHealth.powerMw)}`}
                <br />
                <span className="sensor-details-list__hint">
                  {latestHealth.statusMessage || 'ok'}; received {formatPreciseDateTime(latestHealth.receivedAt)}
                </span>
              </>
            ) : (
              'No health report'
            )}
          </dd>
        </div>

        <div>
          <dt>
            <RefreshCw size={15} />
            <span>Last device sync</span>
          </dt>
          <dd>
            {latestTimeSync ? (
              <>
                {formatPreciseDateTime(latestTimeSync.serverTransmitAt)}
                <br />
                <span className="sensor-details-list__hint">
                  recorded {formatPreciseDateTime(latestTimeSync.createdAt)}
                </span>
              </>
            ) : (
              'No device sync events'
            )}
          </dd>
        </div>
      </dl>

      <section className="sensor-sound-events">
        <div className="sensor-sound-events__header">
          <div className="panel-heading">
            <Volume2 size={16} />
            <span>Acoustic events</span>
          </div>

          <button
            type="button"
            className="icon-button"
            onClick={onRefreshSoundEvents}
            title="Refresh acoustic events"
          >
            <RefreshCw size={16} className={soundEventsState.isLoading ? 'is-spinning' : ''} />
          </button>
        </div>

        {soundEventsState.error ? (
          <div className="sensor-sound-events__error">
            {soundEventsState.error.message || 'Failed to load acoustic events.'}
          </div>
        ) : null}

        {!soundEventsState.isLoading && !soundEventsState.items.length ? (
          <p className="panel-empty">No acoustic events for this device.</p>
        ) : null}

        {soundEventsState.items.length ? (
          <div className="sensor-sound-events__list">
            {soundEventsState.items.map((soundEvent) => {
              const audioUrl = getSoundEventAudioUrl(soundEvent);
              const isUploading = soundEventsState.uploadingEventId === soundEvent.id;
              const audioSize = formatBytes(soundEvent.audioSizeBytes);

              return (
                <article key={soundEvent.id} className="sensor-sound-events__item">
                  <div className="sensor-sound-events__details">
                    <strong>{formatPreciseDateTime(soundEvent.eventTime)}</strong>
                    <div className="sensor-sound-events__meta">
                      <span>ID: {soundEvent.id}</span>
                      <span>Peak: {soundEvent.peakLevel ?? 'n/a'}</span>
                      <span>RMS: {soundEvent.rmsLevel ?? 'n/a'}</span>
                    </div>
                    <div className="sensor-sound-events__meta">
                      <span>Received: {formatPreciseDateTime(soundEvent.receivedAt)}</span>
                      <span>{soundEvent.audioUploaded ? 'Audio uploaded' : 'No audio'}</span>
                      {soundEvent.audioContentType ? <span>{soundEvent.audioContentType}</span> : null}
                      {audioSize ? <span>{audioSize}</span> : null}
                    </div>
                    {audioUrl ? (
                      <audio className="sensor-sound-events__player" controls preload="none" src={audioUrl} />
                    ) : null}
                  </div>

                  <div className="sensor-sound-events__actions">
                    {audioUrl ? (
                      <a className="icon-button" href={audioUrl} download title="Download event audio">
                        <Download size={16} />
                      </a>
                    ) : null}

                    <label
                      className={`icon-button${isUploading ? ' icon-button--disabled' : ''}`}
                      aria-disabled={isUploading}
                      title="Upload audio file"
                    >
                      {isUploading ? <RefreshCw size={16} className="is-spinning" /> : <Upload size={16} />}
                      <input
                        className="sensor-sound-events__upload-input"
                        type="file"
                        accept={AUDIO_FILE_ACCEPT}
                        disabled={isUploading}
                        onChange={(changeEvent) => handleAudioUploadChange(soundEvent, changeEvent)}
                      />
                    </label>
                  </div>
                </article>
              );
            })}
          </div>
        ) : null}
      </section>
    </aside>
  );
}
