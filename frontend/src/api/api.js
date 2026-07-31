import axios from 'axios'

const BASE = '/api'

export const api = {
  // Fetch full city graph (call once on mount)
  getGraph: () => axios.get(`${BASE}/graph`).then(r => r.data),

  // Poll live state every 500ms
  getState: () => axios.get(`${BASE}/state`).then(r => r.data),

  // Detailed analytics
  getAnalytics: () => axios.get(`${BASE}/analytics`).then(r => r.data),

  // Simulation controls
  start:  () => axios.post(`${BASE}/sim/start`),
  stop:   () => axios.post(`${BASE}/sim/stop`),
  pause:  () => axios.post(`${BASE}/sim/pause`),
  resume: () => axios.post(`${BASE}/sim/resume`),
  reset:  () => axios.post(`${BASE}/sim/reset`),

  setSpeed: (mult) => axios.post(`${BASE}/sim/speed`, { multiplier: mult }),

  turbo:  (hours = 8) => axios.post(`${BASE}/sim/turbo`, { hours }),
}
