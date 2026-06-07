import { Settings } from 'lucide-react';
import { useCallback, useEffect, useMemo, useState } from 'react';
import { appConfig } from '@/app/config/env';
import { useIncidentFeed } from '@/entities/incident/model/useIncidentFeed';
import { useIncidentNotifications } from '@/entities/incident/model/useIncidentNotifications';
import { useSensors } from '@/entities/sensor/model/useSensors';
import { SensorDetailsDrawer } from '@/entities/sensor/ui/SensorDetailsDrawer';
import { listDeviceSoundEvents, uploadSoundEventAudioFile } from '@/entities/sound-event/api/soundEventApi';
import { OperationsMap } from '@/widgets/operations-map/OperationsMap';
import { NotificationStack } from '@/widgets/operations-notifications/NotificationStack';

const HIDDEN_INCIDENTS_STORAGE_KEY = 'ams.operations.hiddenIncidentKeys';

const getIncidentVisibilityKey = (incident) => `${incident.id}:${incident.createdAt}`;

const getIncidentNotificationId = (incident) => `${incident.id}-${incident.createdAt}`;

const readHiddenIncidentKeys = () => {
  if (typeof window === 'undefined') {
    return new Set();
  }

  try {
    const parsed = JSON.parse(window.localStorage.getItem(HIDDEN_INCIDENTS_STORAGE_KEY) || '[]');
    return new Set(Array.isArray(parsed) ? parsed.map(String) : []);
  } catch {
    return new Set();
  }
};

const writeHiddenIncidentKeys = (incidentKeys) => {
  if (typeof window === 'undefined') {
    return;
  }

  window.localStorage.setItem(HIDDEN_INCIDENTS_STORAGE_KEY, JSON.stringify([...incidentKeys]));
};

export function OperationsConsole({ onOpenAdmin }) {
  const [viewState, setViewState] = useState(appConfig.mapView);
  const [selectedIncidentId, setSelectedIncidentId] = useState(null);
  const [selectedSensorId, setSelectedSensorId] = useState(null);
  const [hiddenIncidentKeys, setHiddenIncidentKeys] = useState(readHiddenIncidentKeys);
  const [soundEventsState, setSoundEventsState] = useState({
    error: null,
    isLoading: false,
    items: [],
    uploadingEventId: null,
  });

  const {
    incidents: suspiciousIncidents,
    isLoading: incidentsLoading,
  } = useIncidentFeed();
  const {
    sensors,
  } = useSensors();
  const visibleIncidents = useMemo(
    () => suspiciousIncidents.filter((incident) => !hiddenIncidentKeys.has(getIncidentVisibilityKey(incident))),
    [hiddenIncidentKeys, suspiciousIncidents],
  );
  const {
    dismissNotification,
    notifications,
  } = useIncidentNotifications(visibleIncidents, { isLoading: incidentsLoading });

  const selectedIncident = useMemo(
    () => visibleIncidents.find((incident) => incident.id === selectedIncidentId) || null,
    [selectedIncidentId, visibleIncidents],
  );
  const selectedSensor = useMemo(
    () => sensors.find((sensor) => sensor.id === selectedSensorId) || null,
    [selectedSensorId, sensors],
  );

  const loadSelectedSensorEvents = useCallback(async (deviceId, options = {}) => {
    if (!deviceId) {
      setSoundEventsState({ error: null, isLoading: false, items: [], uploadingEventId: null });
      return;
    }

    setSoundEventsState((currentState) => ({
      ...currentState,
      error: null,
      isLoading: true,
    }));

    try {
      const items = await listDeviceSoundEvents(deviceId, {
        limit: 20,
        signal: options.signal,
      });

      setSoundEventsState({
        error: null,
        isLoading: false,
        items,
        uploadingEventId: null,
      });
    } catch (error) {
      if (error.name === 'AbortError') {
        return;
      }

      setSoundEventsState((currentState) => ({
        ...currentState,
        error,
        isLoading: false,
        uploadingEventId: null,
      }));
    }
  }, []);

  useEffect(() => {
    if (!selectedSensorId) {
      setSoundEventsState({ error: null, isLoading: false, items: [], uploadingEventId: null });
      return undefined;
    }

    const controller = new AbortController();
    loadSelectedSensorEvents(selectedSensorId, { signal: controller.signal });

    return () => controller.abort();
  }, [loadSelectedSensorEvents, selectedSensorId]);

  const clearMapSelection = () => {
    setSelectedIncidentId(null);
    setSelectedSensorId(null);
  };

  const uploadSelectedSensorEventAudio = async (eventId, file) => {
    if (!selectedSensor?.id || !eventId || !file) {
      return;
    }

    setSoundEventsState((currentState) => ({
      ...currentState,
      error: null,
      uploadingEventId: eventId,
    }));

    try {
      await uploadSoundEventAudioFile(selectedSensor.id, eventId, file);
      await loadSelectedSensorEvents(selectedSensor.id);
    } catch (error) {
      setSoundEventsState((currentState) => ({
        ...currentState,
        error,
        uploadingEventId: null,
      }));
    }
  };

  const hideIncident = (incident) => {
    if (!incident) {
      return;
    }

    const incidentKey = getIncidentVisibilityKey(incident);
    setHiddenIncidentKeys((currentKeys) => {
      const nextKeys = new Set(currentKeys);
      nextKeys.add(incidentKey);
      writeHiddenIncidentKeys(nextKeys);
      return nextKeys;
    });
    dismissNotification(getIncidentNotificationId(incident));
    setSelectedIncidentId(null);
  };

  return (
    <div className="app-shell">
      <div className="app-shell__map-layer">
        <OperationsMap
          incidents={visibleIncidents}
          onClearSelection={clearMapSelection}
          onHideIncident={hideIncident}
          onSelectIncident={(incident) => {
            setSelectedIncidentId(incident.id);
            setSelectedSensorId(null);
          }}
          onSelectSensor={(sensor) => {
            setSelectedSensorId(sensor.id);
            setSelectedIncidentId(null);
          }}
          onViewStateChange={setViewState}
          selectedIncident={selectedIncident}
          selectedSensor={selectedSensor}
          sensors={sensors}
          viewState={viewState}
        />
      </div>

      <button
        type="button"
        className="operations-admin-button"
        onClick={onOpenAdmin}
        aria-label="Open admin panel"
        title="Open admin panel"
      >
        <Settings size={18} />
      </button>

      <SensorDetailsDrawer
        sensor={selectedSensor}
        soundEventsState={soundEventsState}
        onClose={() => setSelectedSensorId(null)}
        onRefreshSoundEvents={() => loadSelectedSensorEvents(selectedSensor?.id)}
        onUploadSoundEventAudio={uploadSelectedSensorEventAudio}
      />

      <NotificationStack
        notifications={notifications}
        onDismiss={dismissNotification}
      />
    </div>
  );
}
