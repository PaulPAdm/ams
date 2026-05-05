import { useEffect, useEffectEvent } from 'react';

export function useInterval(callback, delay, enabled = true) {
  const onTick = useEffectEvent(callback);

  useEffect(() => {
    if (!enabled || delay === null || delay === undefined) {
      return undefined;
    }

    const intervalId = window.setInterval(() => {
      onTick();
    }, delay);

    return () => {
      window.clearInterval(intervalId);
    };
  }, [delay, enabled]);
}
