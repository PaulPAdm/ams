import { startTransition, useEffect, useState } from 'react';
import { listSensors } from '@/entities/sensor/api/sensorApi';
import { FALLBACK_SENSORS } from '@/entities/sensor/model/sensor';

const initialState = {
  sensors: FALLBACK_SENSORS,
  error: null,
  isLoading: true,
  source: 'fallback',
};

export function useSensors() {
  const [state, setState] = useState(initialState);

  useEffect(() => {
    let disposed = false;

    async function loadSensors() {
      try {
        const sensors = await listSensors();

        if (disposed) {
          return;
        }

        startTransition(() => {
          setState({
            sensors: sensors.length > 0 ? sensors : FALLBACK_SENSORS,
            error: null,
            isLoading: false,
            source: sensors.length > 0 ? 'remote' : 'fallback',
          });
        });
      } catch (error) {
        if (disposed) {
          return;
        }

        startTransition(() => {
          setState({
            sensors: FALLBACK_SENSORS,
            error,
            isLoading: false,
            source: 'fallback',
          });
        });
      }
    }

    loadSensors();

    return () => {
      disposed = true;
    };
  }, []);

  return state;
}
