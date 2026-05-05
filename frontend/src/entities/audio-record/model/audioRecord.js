export function mapAudioRecord(rawRecord) {
  return {
    createdAt: rawRecord.created_at,
    deviceId: String(rawRecord.device_id),
    downloadUrl: rawRecord.download_url || null,
    filePath: rawRecord.file_path,
    id: Number(rawRecord.id),
  };
}

export function getAudioRecordFileName(record) {
  const parts = record.filePath.split('/');
  return parts[parts.length - 1] || record.filePath;
}
