import { startTransition, useEffect, useEffectEvent, useState } from 'react';
import { appConfig } from '@/app/config/env';
import { listIncidents } from '@/entities/incident/api/incidentApi';

const initialState = {
  incidents: [],
  error: null,
  isLoading: true,
  lastUpdated: null,
};

export function useIncidentFeed() {
  const [state, setState] = useState(initialState);

  const loadIncidents = useEffectEvent(async () => {
    try {
      const incidents = await listIncidents();

      startTransition(() => {
        setState({
          incidents,
          error: null,
          isLoading: false,
          lastUpdated: new Date().toISOString(),
        });
      });
    } catch (error) {
      startTransition(() => {
        setState((currentState) => ({
          ...currentState,
          error,
          isLoading: false,
        }));
      });
    }
  });

  useEffect(() => {
    loadIncidents();

    const intervalId = window.setInterval(() => {
      loadIncidents();
    }, appConfig.intervals.incidentsMs);

    return () => {
      window.clearInterval(intervalId);
    };
  }, []);

  return state;
}
