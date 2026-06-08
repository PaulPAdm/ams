import { appConfig } from '@/app/config/env';

const clockFormatter = new Intl.DateTimeFormat(appConfig.locale, {
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
});

const dateTimeFormatter = new Intl.DateTimeFormat(appConfig.locale, {
  day: '2-digit',
  month: 'short',
  hour: '2-digit',
  minute: '2-digit',
});

const preciseFormatter = new Intl.DateTimeFormat(appConfig.locale, {
  day: '2-digit',
  month: '2-digit',
  year: 'numeric',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hour12: false,
});

export function formatClock(value) {
  if (!value) {
    return '--:--:--';
  }

  return clockFormatter.format(new Date(value));
}

export function formatDateTime(value) {
  if (!value) {
    return 'No timestamp';
  }

  return dateTimeFormatter.format(new Date(value));
}

export function formatPreciseDateTime(value) {
  if (!value) {
    return 'No timestamp';
  }

  const date = new Date(value);
  const milliseconds = String(date.getMilliseconds()).padStart(3, '0');
  return `${preciseFormatter.format(date)}.${milliseconds}`;
}

export function formatCoordinate(value) {
  if (!Number.isFinite(value)) {
    return 'n/a';
  }

  return value.toFixed(4);
}

export function formatDb(value) {
  if (!Number.isFinite(value)) {
    return '-- dB';
  }

  return `${Math.round(value)} dB`;
}

export function formatPercent(value) {
  if (!Number.isFinite(value)) {
    return '--';
  }

  return `${Math.round(value)}%`;
}

export function formatVoltage(value) {
  if (!Number.isFinite(value)) {
    return 'Unknown';
  }

  return `${value.toFixed(2)} V`;
}

export function formatCurrent(value) {
  if (!Number.isFinite(value)) {
    return 'Unknown';
  }

  return `${Math.round(value)} mA`;
}

export function formatPower(value) {
  if (!Number.isFinite(value)) {
    return 'Unknown';
  }

  return `${Math.round(value)} mW`;
}

export function formatBytes(value) {
  if (!Number.isFinite(value) || value <= 0) {
    return null;
  }

  if (value < 1024) {
    return `${value} B`;
  }

  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }

  return `${(value / (1024 * 1024)).toFixed(1)} MB`;
}

const UNKNOWN_VALUES = new Set(['', 'unknown', 'unknow', 'n/a', 'na', 'none', 'null']);

export function toBoolOrNull(value) {
  if (typeof value === 'boolean') {
    return value;
  }

  if (value === null || value === undefined) {
    return null;
  }

  const normalized = String(value).trim().toLowerCase();
  if (UNKNOWN_VALUES.has(normalized)) {
    return null;
  }

  if (normalized === 'true') {
    return true;
  }

  if (normalized === 'false') {
    return false;
  }

  return null;
}
