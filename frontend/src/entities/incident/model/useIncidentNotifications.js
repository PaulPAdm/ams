import { useEffect, useRef, useState } from 'react';
import { appConfig } from '@/app/config/env';

const MAX_OBSERVED_IDS = 500;

const createNotification = (incident) => ({
  id: `${incident.id}-${incident.createdAt}`,
  title: 'New triangulated point',
  message: incident.description,
  createdAt: incident.createdAt,
  addedAt: Date.now(),
});

export function useIncidentNotifications(incidents, { isLoading = false } = {}) {
  const observedIncidentIdsRef = useRef(null);
  const [notifications, setNotifications] = useState([]);

  useEffect(() => {
    if (isLoading) {
      return;
    }

    if (observedIncidentIdsRef.current === null) {
      observedIncidentIdsRef.current = new Set(incidents.map((incident) => incident.id));
      return;
    }

    const newIncidents = incidents.filter(
      (incident) => !observedIncidentIdsRef.current.has(incident.id),
    );

    incidents.forEach((incident) => {
      observedIncidentIdsRef.current.add(incident.id);
    });

    // Bound the Set so it doesn't grow forever over long sessions.
    if (observedIncidentIdsRef.current.size > MAX_OBSERVED_IDS) {
      const entries = [...observedIncidentIdsRef.current];
      observedIncidentIdsRef.current = new Set(entries.slice(-MAX_OBSERVED_IDS));
    }

    if (!newIncidents.length) {
      return;
    }

    setNotifications((currentNotifications) =>
      [
        ...newIncidents.map(createNotification),
        ...currentNotifications,
      ].slice(0, appConfig.alerts.maxNotifications),
    );
  }, [incidents, isLoading]);

  // Dismiss the oldest notification after its display time expires.
  // Each render of this effect cancels the previous timeout so only one
  // timeout is active at a time — avoids stacked timeouts from rapid
  // notifications arriving in sequence.
  useEffect(() => {
    if (!notifications.length) {
      return undefined;
    }

    const oldest = notifications[notifications.length - 1];
    const elapsed = Date.now() - oldest.addedAt;
    const remaining = Math.max(0, appConfig.alerts.dismissMs - elapsed);

    const timeoutId = window.setTimeout(() => {
      setNotifications((current) => current.slice(0, -1));
    }, remaining);

    return () => {
      window.clearTimeout(timeoutId);
    };
  }, [notifications]);

  const dismissNotification = (notificationId) => {
    setNotifications((currentNotifications) =>
      currentNotifications.filter((notification) => notification.id !== notificationId),
    );
  };

  return {
    dismissNotification,
    notifications,
  };
}
