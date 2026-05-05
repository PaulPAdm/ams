import { startTransition, useCallback, useEffect, useMemo, useState } from 'react';
import { appConfig } from '@/app/config/env';
import {
  clearDeviceAudioRecords,
  deleteAudioRecord,
  listDeviceAudioRecords,
} from '@/entities/audio-record/api/audioRecordApi';
import {
  createDevice,
  deleteDevice,
  initializeDevice,
  listDevices,
  updateDevice,
} from '@/entities/device/api/deviceApi';
import { clearIncidents, deleteIncident, listIncidents } from '@/entities/incident/api/incidentApi';
import { clearPeaks, deletePeak, listDevicePeaks, listPeaks } from '@/entities/peak/api/peakApi';
import { getJson } from '@/shared/api/http';
import { useInterval } from '@/shared/lib/useInterval';

const initialPeaksState = {
  deviceId: null,
  error: null,
  isGlobal: false,
  isLoading: false,
  isOpen: false,
  items: [],
};

const initialAudioState = {
  deviceId: null,
  error: null,
  isLoading: false,
  isOpen: false,
  items: [],
};

export function useAdminConsole() {
  const [audioState, setAudioState] = useState(initialAudioState);
  const [createdDeviceId, setCreatedDeviceId] = useState(null);
  const [devices, setDevices] = useState([]);
  const [devicesError, setDevicesError] = useState(null);
  const [formDevice, setFormDevice] = useState(undefined);
  const [incidents, setIncidents] = useState([]);
  const [incidentsError, setIncidentsError] = useState(null);
  const [isBootstrapping, setIsBootstrapping] = useState(true);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [isSavingDevice, setIsSavingDevice] = useState(false);
  const [peaksState, setPeaksState] = useState(initialPeaksState);
  const [systemHealth, setSystemHealth] = useState('Checking...');

  const loadDevices = useCallback(async () => {
    try {
      const nextDevices = await listDevices();
      startTransition(() => {
        setDevices(nextDevices);
        setDevicesError(null);
      });
    } catch (error) {
      startTransition(() => {
        setDevicesError(error);
      });
    }
  }, []);

  const loadIncidents = useCallback(async () => {
    try {
      const nextIncidents = await listIncidents({ includeUnlocated: true });
      startTransition(() => {
        setIncidents(nextIncidents);
        setIncidentsError(null);
      });
    } catch (error) {
      startTransition(() => {
        setIncidentsError(error);
      });
    }
  }, []);

  const checkHealth = useCallback(async () => {
    try {
      await getJson('/api/health');
      startTransition(() => {
        setSystemHealth('Online');
      });
    } catch {
      startTransition(() => {
        setSystemHealth('Offline');
      });
    }
  }, []);

  const refreshDashboard = useCallback(async (options = {}) => {
    const { initial = false } = options;

    if (initial) {
      setIsBootstrapping(true);
    } else {
      setIsRefreshing(true);
    }

    await Promise.allSettled([loadDevices(), loadIncidents(), checkHealth()]);

    if (initial) {
      setIsBootstrapping(false);
    } else {
      setIsRefreshing(false);
    }
  }, [checkHealth, loadDevices, loadIncidents]);

  useEffect(() => {
    refreshDashboard({ initial: true });
  }, [refreshDashboard]);

  useInterval(() => {
    refreshDashboard();
  }, appConfig.intervals.adminRefreshMs, !isBootstrapping);

  const openCreateForm = () => {
    setFormDevice(null);
  };

  const openEditForm = (device) => {
    setFormDevice(device);
  };

  const closeForm = () => {
    setFormDevice(undefined);
  };

  const formMode = formDevice === undefined ? 'closed' : formDevice ? 'edit' : 'create';
  const isFormOpen = formMode !== 'closed';

  const saveDevice = async (payload) => {
    setIsSavingDevice(true);

    try {
      if (formMode === 'edit' && formDevice) {
        await updateDevice(formDevice.id, payload);
      } else {
        const createdDevice = await createDevice(payload);
        setCreatedDeviceId(createdDevice.id);
      }

      closeForm();
      await loadDevices();
    } finally {
      setIsSavingDevice(false);
    }
  };

  const deleteDeviceById = async (deviceId) => {
    await deleteDevice(deviceId);
    await loadDevices();

    setPeaksState((currentState) => (
      currentState.deviceId === deviceId
        ? initialPeaksState
        : currentState
    ));
    setAudioState((currentState) => (
      currentState.deviceId === deviceId
        ? initialAudioState
        : currentState
    ));
  };

  const initializeDeviceById = (deviceId) => initializeDevice(deviceId);

  const loadDrawerPeaks = async (deviceId = null) => {
    setPeaksState((currentState) => ({
      ...currentState,
      deviceId,
      error: null,
      isGlobal: !deviceId,
      isLoading: true,
      isOpen: true,
    }));

    try {
      const items = deviceId ? await listDevicePeaks(deviceId) : await listPeaks();
      setPeaksState({
        deviceId,
        error: null,
        isGlobal: !deviceId,
        isLoading: false,
        isOpen: true,
        items,
      });
    } catch (error) {
      setPeaksState((currentState) => ({
        ...currentState,
        error,
        isLoading: false,
      }));
    }
  };

  const loadDrawerAudio = async (deviceId) => {
    setAudioState({
      deviceId,
      error: null,
      isLoading: true,
      isOpen: true,
      items: [],
    });

    try {
      const items = await listDeviceAudioRecords(deviceId);
      setAudioState({
        deviceId,
        error: null,
        isLoading: false,
        isOpen: true,
        items,
      });
    } catch (error) {
      setAudioState((currentState) => ({
        ...currentState,
        error,
        isLoading: false,
      }));
    }
  };

  const removePeakById = async (peakId) => {
    await deletePeak(peakId);
    setPeaksState((currentState) => ({
      ...currentState,
      items: currentState.items.filter((peak) => peak.id !== peakId),
    }));
  };

  const clearAllPeaks = async () => {
    await clearPeaks();
    setPeaksState((currentState) => ({
      ...currentState,
      items: [],
    }));
  };

  const removeAudioRecordById = async (recordId) => {
    await deleteAudioRecord(recordId);
    setAudioState((currentState) => ({
      ...currentState,
      items: currentState.items.filter((record) => record.id !== recordId),
    }));
  };

  const clearAudioRecordsForDevice = async (deviceId) => {
    await clearDeviceAudioRecords(deviceId);
    setAudioState((currentState) => ({
      ...currentState,
      items: [],
    }));
  };

  const removeIncidentById = async (incidentId) => {
    await deleteIncident(incidentId);
    setIncidents((currentIncidents) => currentIncidents.filter((incident) => incident.id !== String(incidentId)));
  };

  const clearAllIncidents = async () => {
    await clearIncidents();
    setIncidents([]);
  };

  const sortedDevices = useMemo(
    () => [...devices].sort((left, right) => left.name.localeCompare(right.name)),
    [devices],
  );

  const sortedIncidents = useMemo(
    () => [...incidents].sort((left, right) => new Date(right.createdAt) - new Date(left.createdAt)),
    [incidents],
  );

  return {
    audioState,
    clearAllIncidents,
    clearAllPeaks,
    clearAudioRecordsForDevice,
    closeAudioDrawer: () => setAudioState(initialAudioState),
    closeCreatedDeviceModal: () => setCreatedDeviceId(null),
    closeForm,
    closePeaksDrawer: () => setPeaksState(initialPeaksState),
    createdDeviceId,
    deleteDeviceById,
    devices: sortedDevices,
    devicesError,
    formDevice: formMode === 'edit' ? formDevice : null,
    incidents: sortedIncidents,
    incidentsError,
    initializeDeviceById,
    isBootstrapping,
    isFormOpen,
    isRefreshing,
    isSavingDevice,
    openCreateForm,
    openDeviceAudioDrawer: loadDrawerAudio,
    openDevicePeaksDrawer: (deviceId) => loadDrawerPeaks(deviceId),
    openEditForm,
    openGlobalPeaksDrawer: () => loadDrawerPeaks(),
    peaksState,
    refreshAudioDrawer: () => audioState.deviceId ? loadDrawerAudio(audioState.deviceId) : undefined,
    refreshDashboard,
    refreshPeaksDrawer: () => loadDrawerPeaks(peaksState.isGlobal ? null : peaksState.deviceId),
    removeAudioRecordById,
    removeIncidentById,
    removePeakById,
    saveDevice,
    systemHealth,
  };
}
