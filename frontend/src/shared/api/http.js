import { appConfig } from '@/app/config/env';

const appendParams = (url, params) => {
  if (!params) {
    return;
  }

  Object.entries(params).forEach(([key, value]) => {
    if (value === undefined || value === null || value === '') {
      return;
    }

    url.searchParams.set(key, String(value));
  });
};

export const buildApiUrl = (path, params) => {
  if (appConfig.apiBaseUrl) {
    const base = appConfig.apiBaseUrl.endsWith('/')
      ? appConfig.apiBaseUrl
      : `${appConfig.apiBaseUrl}/`;
    const url = new URL(path, base);
    appendParams(url, params);
    return url.toString();
  }

  const origin = typeof window === 'undefined' ? 'http://localhost' : window.location.origin;
  const url = new URL(path, origin);
  appendParams(url, params);
  return `${url.pathname}${url.search}`;
};

const parseResponse = async (response) => {
  if (response.status === 204) {
    return null;
  }

  const contentType = response.headers.get('content-type') || '';

  if (contentType.includes('application/json')) {
    return response.json();
  }

  return response.text();
};

export async function requestJson(path, options = {}) {
  const { body, headers, method = 'GET', params, signal } = options;
  const response = await fetch(buildApiUrl(path, params), {
    method,
    headers: {
      Accept: 'application/json',
      ...(body !== undefined ? { 'Content-Type': 'application/json' } : {}),
      ...headers,
    },
    body: body !== undefined ? JSON.stringify(body) : undefined,
    signal,
  });

  if (!response.ok) {
    const errorPayload = await parseResponse(response);
    const message = typeof errorPayload === 'string'
      ? errorPayload
      : errorPayload?.detail || `Request failed with ${response.status}`;
    throw new Error(message);
  }

  return parseResponse(response);
}

export function getJson(path, options = {}) {
  return requestJson(path, {
    ...options,
    method: 'GET',
  });
}

export function postJson(path, body, options = {}) {
  return requestJson(path, {
    ...options,
    method: 'POST',
    body,
  });
}

export async function requestFormData(path, options = {}) {
  const { body, headers, method = 'POST', params, signal } = options;
  const response = await fetch(buildApiUrl(path, params), {
    method,
    headers: {
      Accept: 'application/json',
      ...headers,
    },
    body,
    signal,
  });

  if (!response.ok) {
    const errorPayload = await parseResponse(response);
    const message = typeof errorPayload === 'string'
      ? errorPayload
      : errorPayload?.detail || `Request failed with ${response.status}`;
    throw new Error(message);
  }

  return parseResponse(response);
}

export function postFormData(path, body, options = {}) {
  return requestFormData(path, {
    ...options,
    method: 'POST',
    body,
  });
}

export function putJson(path, body, options = {}) {
  return requestJson(path, {
    ...options,
    method: 'PUT',
    body,
  });
}

export function deleteJson(path, options = {}) {
  return requestJson(path, {
    ...options,
    method: 'DELETE',
  });
}
