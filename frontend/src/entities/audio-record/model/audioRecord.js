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

export function mapAudioRecord(rawRecord) {
  return {
    contentType: rawRecord.audio_content_type || null,
    createdAt: nsToIso(rawRecord.received_at_ns),
    deviceId: String(rawRecord.device_id),
    downloadUrl: rawRecord.audio_download_url || rawRecord.download_url || null,
    eventTime: nsToIso(rawRecord.event_time_ns),
    filePath: rawRecord.audio_file_path || '',
    id: Number(rawRecord.id),
    sizeBytes: Number.isFinite(Number(rawRecord.audio_size_bytes)) ? Number(rawRecord.audio_size_bytes) : null,
  };
}

export function getAudioRecordFileName(record) {
  if (!record.filePath) {
    return `sound-event-${record.id}`;
  }

  const parts = record.filePath.split('/');
  return parts[parts.length - 1] || record.filePath;
}
