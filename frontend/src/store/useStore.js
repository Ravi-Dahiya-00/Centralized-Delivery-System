import { create } from 'zustand'

// Max history points to keep (prevents unbounded memory growth)
const MAX_HISTORY = 120

// ── Rush Hour Info ──────────────────────────────────────────────
export function getRushInfo(simTimeSec) {
  const hour = Math.floor((simTimeSec % 86400) / 3600)
  const min  = Math.floor((simTimeSec % 3600)  / 60)
  const label = `${String(hour).padStart(2,'0')}:${String(min).padStart(2,'0')}`

  if (hour >= 0  && hour < 6)  return { label, phase: 'Night',     mult: 0.2, color: '#475569', emoji: '🌙' }
  if (hour >= 6  && hour < 7)  return { label, phase: 'Morning',   mult: 0.6, color: '#94a3b8', emoji: '🌅' }
  if (hour >= 7  && hour < 10) return { label, phase: 'Breakfast', mult: 1.5, color: '#f59e0b', emoji: '☕' }
  if (hour >= 10 && hour < 12) return { label, phase: 'Midday',    mult: 0.9, color: '#94a3b8', emoji: '🕙' }
  if (hour >= 12 && hour < 15) return { label, phase: 'LUNCH RUSH',mult: 3.0, color: '#e23744', emoji: '🍽️' }
  if (hour >= 15 && hour < 17) return { label, phase: 'Afternoon', mult: 1.2, color: '#f59e0b', emoji: '☀️' }
  if (hour >= 17 && hour < 19) return { label, phase: 'Evening',   mult: 1.0, color: '#94a3b8', emoji: '🌇' }
  if (hour >= 19 && hour < 23) return { label, phase: 'DINNER RUSH',mult:2.5, color: '#fc8019', emoji: '🍕' }
  return                       { label, phase: 'Late Night', mult: 0.4, color: '#475569', emoji: '🌃' }
}

// Zustand global store — holds all live simulation state
const useStore = create((set, get) => ({
  // ── Graph (static, loaded once) ──────────────────────────
  graph: null,
  setGraph: (g) => set({ graph: g }),

  // ── Live state (polled every 500ms) ──────────────────────
  orders: [],
  riders: [],
  analytics: null,
  simState: 'STOPPED',   // RUNNING | PAUSED | STOPPED | TURBO
  simTimeSec: 0,
  speedMultiplier: 1.0,

  setLiveState: (data) => {
    const prev = get()
    const newTimeSec = data.sim_time_sec ?? 0

    // ── Append to history ring buffer ─────────────────────
    let newHistory = prev.analyticsHistory
    if (data.analytics && data.sim_state === 'RUNNING') {
      const point = {
        t:            newTimeSec,
        earnings:     data.analytics.total_earnings_inr     ?? 0,
        distSaved:    data.analytics.total_distance_saved_km ?? 0,
        ordersPlaced: data.analytics.total_orders_placed    ?? 0,
        batchRate:    data.analytics.batch_rate_pct         ?? 0,
        // per-rider earnings snapshot
        riderEarnings: (data.analytics.rider_stats ?? []).map(r => ({
          id:   r.rider_id,
          name: r.rider_name,
          v:    r.earnings_inr ?? 0,
        })),
      }
      newHistory = [...prev.analyticsHistory, point].slice(-MAX_HISTORY)
    }

    set({
      orders:           data.orders          ?? [],
      riders:           data.riders          ?? [],
      analytics:        data.analytics       ?? null,
      simState:         data.sim_state       ?? 'STOPPED',
      simTimeSec:       newTimeSec,
      speedMultiplier:  data.speed_multiplier ?? 1.0,
      analyticsHistory: newHistory,
    })
  },

  // ── History ring buffer ───────────────────────────────────
  analyticsHistory: [],   // Array of { t, earnings, distSaved, ordersPlaced, batchRate, riderEarnings }
  clearHistory: () => set({ analyticsHistory: [] }),

  // ── UI State ─────────────────────────────────────────────
  activeTab:       'map',
  setActiveTab:    (tab) => set({ activeTab: tab }),

  selectedOrderId: null,
  setSelectedOrder: (id) => set({ selectedOrderId: id }),

  selectedRiderId: null,
  setSelectedRider: (id) => set({ selectedRiderId: id }),

  // ── Recent Events Log ─────────────────────────────────────
  eventLog: [],
  addEvent: (msg) => set((state) => ({
    eventLog: [{ msg, ts: Date.now() }, ...state.eventLog].slice(0, 50)
  })),

  // ── Helpers ──────────────────────────────────────────────
  getRiderById:  (id) => get().riders.find(r => r.id === id),
  getOrderById:  (id) => get().orders.find(o => o.id === id),

  // Format simulation time as HH:MM:SS
  formattedSimTime: () => {
    const s = get().simTimeSec
    const h = Math.floor(s / 3600)
    const m = Math.floor((s % 3600) / 60)
    const sec = s % 60
    return `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`
  }
}))

export default useStore
