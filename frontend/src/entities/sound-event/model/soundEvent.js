const nsToIso = (value) => {
  if (value === null || value === undefined || value === '') {
    return null;
  }

  try {
    const milliseconds = BigInt(String(value)) / 1_000_000n;
    return new Date(Number(milliseconds)).toISOString();
  } catch {
    const parsed = Number(value);

    if (!Number.isFinite(parsed)) {
      return null;
    }

    return new Date(parsed / 1_000_000).toISOString();
  }
};

const toNumberOrNull = (value) => {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
};

export function mapSoundEvent(rawEvent) {
  return {
    id: Number(rawEvent.id),
    deviceId: String(rawEvent.device_id),
    eventTime: nsToIso(rawEvent.event_time_ns),
    receivedAt: nsToIso(rawEvent.received_at_ns),
    peakLevel: toNumberOrNull(rawEvent.peak_level),
    rmsLevel: toNumberOrNull(rawEvent.rms_level),
    noiseFloor: toNumberOrNull(rawEvent.noise_floor),
    audioUploaded: Boolean(rawEvent.audio_uploaded),
    audioDownloadUrl: rawEvent.audio_download_url || null,
    audioContentType: rawEvent.audio_content_type || null,
    audioSizeBytes: toNumberOrNull(rawEvent.audio_size_bytes),
    label: rawEvent.detection_label || 'impulse',
  };
}
