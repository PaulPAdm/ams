import { useCallback, useEffect, useMemo, useState } from 'react';
import { listDeviceHealthReports, listDevices } from '@/entities/device/api/deviceApi';

const HEALTH_REPORT_LIMIT = 200;

const readDeviceFromQuery = () => {
  if (typeof window === 'undefined') {
    return null;
  }

  return new URLSearchParams(window.location.search).get('device');
};

export function useDeviceHealth() {
  const [devices, setDevices] = useState([]);
  const [devicesError, setDevicesError] = useState(null);
  const [selectedDeviceId, setSelectedDeviceId] = useState(readDeviceFromQuery);
  const [reports, setReports] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);

  useEffect(() => {
    let active = true;

    listDevices()
      .then((nextDevices) => {
        if (!active) {
          return;
        }

        setDevices(nextDevices);
        setDevicesError(null);
        setSelectedDeviceId((current) => {
          if (current && nextDevices.some((device) => device.id === current)) {
            return current;
          }

          return nextDevices[0]?.id ?? null;
        });
      })
      .catch((nextError) => {
        if (active) {
          setDevices([]);
          setDevicesError(nextError);
        }
      });

    return () => {
      active = false;
    };
  }, []);

  const loadReports = useCallback((deviceId, signal) => {
    if (!deviceId) {
      setReports([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    listDeviceHealthReports(deviceId, { limit: HEALTH_REPORT_LIMIT, signal })
      .then((nextReports) => {
        setReports(nextReports);
        setIsLoading(false);
      })
      .catch((nextError) => {
        if (nextError.name === 'AbortError') {
          return;
        }

        setError(nextError);
        setReports([]);
        setIsLoading(false);
      });
  }, []);

  useEffect(() => {
    const controller = new AbortController();
    // Intentional load-start on device change; resolution updates state in callbacks.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    loadReports(selectedDeviceId, controller.signal);
    return () => controller.abort();
  }, [loadReports, selectedDeviceId]);

  const selectDevice = useCallback((deviceId) => {
    setSelectedDeviceId(deviceId || null);

    if (typeof window !== 'undefined') {
      const nextPath = deviceId ? `/admin/health?device=${encodeURIComponent(deviceId)}` : '/admin/health';
      window.history.replaceState({}, '', nextPath);
    }
  }, []);

  const refresh = useCallback(() => {
    loadReports(selectedDeviceId);
  }, [loadReports, selectedDeviceId]);

  const summary = useMemo(() => {
    const latest = reports.length ? reports[reports.length - 1] : null;
    const powerValues = reports
      .map((report) => report.powerMw)
      .filter((value) => Number.isFinite(value));
    const avgPowerMw = powerValues.length
      ? powerValues.reduce((sum, value) => sum + value, 0) / powerValues.length
      : null;
    const droppedTotal = reports.reduce(
      (max, report) => (Number.isFinite(report.audioDroppedChunks) ? Math.max(max, report.audioDroppedChunks) : max),
      0,
    );

    return { avgPowerMw, droppedTotal, latest };
  }, [reports]);

  const selectedDevice = useMemo(
    () => devices.find((device) => device.id === selectedDeviceId) || null,
    [devices, selectedDeviceId],
  );

  return {
    devices,
    devicesError,
    error,
    isLoading,
    refresh,
    reports,
    selectDevice,
    selectedDevice,
    selectedDeviceId,
    summary,
  };
}
