export function mapPeak(rawPeak) {
  return {
    deviceId: String(rawPeak.device_id),
    id: Number(rawPeak.id),
    peakTime: rawPeak.peak_time,
    receivedAt: rawPeak.received_at,
  };
}
