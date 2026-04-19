import axios from 'axios';

const api = axios.create({
  baseURL: '', // Requests will go to the current host (Vite proxy)
});

export const locationApi = {
  list: (skip = 0, limit = 1000) => api.get('/api/locations/', { params: { skip, limit } }),
  get: (id) => api.get(`/api/devices/${id}`),
  create: (data) => api.post('/api/locations/', data),
  update: (id, data) => api.put(`/api/locations/${id}`, data),
  delete: (id) => api.delete(`/api/locations/${id}`),
  init: (id) => api.post(`/api/locations/${id}/init`),
};

export const peakApi = {
  list: (skip = 0, limit = 1000) => api.get('/api/peaks/', { params: { skip, limit } }),
  getByDevice: (deviceId, skip = 0, limit = 1000) => 
    api.get(`/api/devices/${deviceId}/peaks`, { params: { skip, limit } }),
  delete: (id) => api.delete(`/api/peaks/${id}`),
  clearAll: () => api.delete('/api/peaks/'),
};

export const suspiciousIncidentsApi = {
  list: (skip = 0, limit = 1000) => api.get('/api/suspicious_incidents/', { params: { skip, limit } }),
  get: (id) => api.get(`/api/suspicious_incidents/${id}`),
  delete: (id) => api.delete(`/api/suspicious_incidents/${id}`),
  clearAll: () => api.delete('/api/suspicious_incidents/'),
};

export const audioRecordApi = {
  get: (id) => api.get(`/api/audio_records/${id}`),
  listByDevice: (deviceId, skip = 0, limit = 1000) => 
    api.get(`/api/devices/${deviceId}/audio_records`, { params: { skip, limit } }),
  delete: (id) => api.delete(`/api/audio_records/${id}`),
  deleteAllByDevice: (deviceId) => api.delete(`/api/devices/${deviceId}/audio_records`),
  downloadUrl: (id) => `/api/audio_records/${id}/download`,
};

export const healthApi = {
  check: () => api.get('/api/health'),
};

export default api;
