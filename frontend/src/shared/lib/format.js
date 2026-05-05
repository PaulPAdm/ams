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
