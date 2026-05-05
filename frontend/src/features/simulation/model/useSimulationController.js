import { useEffect, useState } from 'react';
import * as turf from '@turf/turf';
import { appConfig } from '@/app/config/env';
import { buildSimulationIncident } from '@/entities/incident/model/incident';

const createNotification = (incident) => ({
  id: `${incident.id}-notification`,
  title: incident.classification,
  message: `${incident.confidence}% confidence. ${incident.activeSensors.length} nodes locked on target.`,
  createdAt: incident.createdAt,
});

export function useSimulationController({ sensors }) {
  const [isEnabled, setIsEnabled] = useState(appConfig.simulation.enabledByDefault);
  const [activeIncident, setActiveIncident] = useState(null);
  const [history, setHistory] = useState([]);
  const [notifications, setNotifications] = useState([]);

  useEffect(() => {
    if (!notifications.length) {
      return undefined;
    }

    const timeoutId = window.setTimeout(() => {
      setNotifications((currentNotifications) => currentNotifications.slice(0, -1));
    }, appConfig.alerts.dismissMs);

    return () => {
      window.clearTimeout(timeoutId);
    };
  }, [notifications]);

  const handleMapClick = ({ lng, lat }) => {
    if (!isEnabled || sensors.length < 3) {
      return;
    }

    const activeSensors = [...sensors]
      .map((sensor) => ({
        ...sensor,
        distance: turf.distance(
          turf.point([lng, lat]),
          turf.point([sensor.lng, sensor.lat]),
        ),
      }))
      .sort((left, right) => left.distance - right.distance)
      .slice(0, 3);

    const incident = buildSimulationIncident({
      lat,
      lng,
      activeSensors,
      minConfidence: appConfig.simulation.minConfidence,
      maxConfidence: appConfig.simulation.maxConfidence,
    });

    setActiveIncident(incident);
    setHistory((currentHistory) =>
      [incident, ...currentHistory].slice(0, appConfig.simulation.historyLimit),
    );
    setNotifications((currentNotifications) =>
      [createNotification(incident), ...currentNotifications].slice(0, appConfig.alerts.maxNotifications),
    );
  };

  const toggleEnabled = () => {
    setIsEnabled((currentValue) => {
      const nextValue = !currentValue;

      if (!nextValue) {
        setActiveIncident(null);
      }

      return nextValue;
    });
  };

  const dismissActiveIncident = () => {
    setActiveIncident(null);
  };

  const dismissNotification = (notificationId) => {
    setNotifications((currentNotifications) =>
      currentNotifications.filter((notification) => notification.id !== notificationId),
    );
  };

  return {
    activeIncident,
    dismissActiveIncident,
    dismissNotification,
    handleMapClick,
    history,
    isEnabled,
    notifications,
    toggleEnabled,
  };
}
