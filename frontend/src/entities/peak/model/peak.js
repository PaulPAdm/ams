const nsToIso = (value) => {
  if (value === null || value === undefined) {
    return null;
  }
  try {
    const ms = BigInt(String(value)) / 1_000_000n;
    return new Date(Number(ms)).toISOString();
  } catch {
    return null;
  }
};

export function mapPeak(rawPeak) {
  return {
    deviceId: String(rawPeak.device_id),
    id: Number(rawPeak.id),
    peakTime: nsToIso(rawPeak.peak_time_ns),
    processed: Boolean(rawPeak.processed),
    receivedAt: nsToIso(rawPeak.received_at_ns),
  };
}
