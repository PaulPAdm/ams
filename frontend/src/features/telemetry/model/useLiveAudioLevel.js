import { useState } from 'react';
import { useInterval } from '@/shared/lib/useInterval';

const randomBetween = (min, max) => {
  if (max <= min) {
    return min;
  }

  return Math.round(Math.random() * (max - min) + min);
};

export function useLiveAudioLevel({ intervalMs, minDb, maxDb }) {
  const [audioLevel, setAudioLevel] = useState(() => randomBetween(minDb, maxDb));

  useInterval(() => {
    setAudioLevel(randomBetween(minDb, maxDb));
  }, intervalMs);

  return audioLevel;
}
