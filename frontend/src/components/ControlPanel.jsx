import React, { useState } from 'react'
import { api } from '../api/api'
import useStore from '../store/useStore'

const SPEEDS = [
  { label: '1×',  value: 1   },
  { label: '2×',  value: 2   },
  { label: '5×',  value: 5   },
  { label: '10×', value: 10  },
]

export default function ControlPanel() {
  const { simState, speedMultiplier, analytics, eventLog } = useStore()
  const [turboHours, setTurboHours] = useState(8)
  const [turboLoading, setTurboLoading] = useState(false)

  const isRunning = simState === 'RUNNING'
  const isStopped = simState === 'STOPPED'

  const handleStart   = () => api.start()
  const handleStop    = () => api.stop()
  const handlePause   = () => isRunning ? api.pause() : api.resume()
  const handleReset   = () => api.reset()
  const handleSpeed   = (v) => api.setSpeed(v)
  const handleTrigger = () => api.triggerOrder()

  const handleTurbo = async () => {
    setTurboLoading(true)
    try {
      await api.turbo(turboHours)
    } finally {
      setTurboLoading(false)
    }
  }

  const fmt = (n, dec = 1) => (n ?? 0).toFixed(dec)

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 20 }}>

      {/* ── Simulation Controls ── */}
      <div className="card">
        <div className="section-title">Simulation Controls</div>

        <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', marginBottom: 20 }}>
          <button className="btn btn-success" onClick={handleStart} disabled={isRunning}>
            ▶ Start
          </button>
          <button
            className={`btn ${isRunning ? 'btn-warning' : 'btn-ghost'}`}
            onClick={handlePause}
            disabled={isStopped}
          >
            {isRunning ? '⏸ Pause' : '▶ Resume'}
          </button>
          <button className="btn btn-danger" onClick={handleStop} disabled={isStopped}>
            ⏹ Stop
          </button>
          <button className="btn btn-ghost" onClick={handleReset}>
            ↺ Reset
          </button>
          <button className="btn btn-primary" onClick={handleTrigger} disabled={!isRunning}>
            + Manual Order
          </button>
        </div>

        {/* Speed Control */}
        <div className="section-title" style={{ marginTop: 12 }}>Simulation Speed</div>
        <div style={{ display: 'flex', gap: 6 }}>
          {SPEEDS.map(s => (
            <button
              key={s.value}
              className={`btn ${Math.abs(speedMultiplier - s.value) < 0.1 ? 'btn-primary' : 'btn-ghost'}`}
              onClick={() => handleSpeed(s.value)}
              style={{ flex: 1 }}
            >
              {s.label}
            </button>
          ))}
        </div>

        <div style={{
          marginTop: 16,
          padding: '10px 14px',
          background: 'rgba(99,102,241,0.08)',
          borderRadius: 8,
          border: '1px solid rgba(99,102,241,0.2)',
          fontSize: 12,
          color: 'var(--text-secondary)'
        }}>
          <b style={{ color: 'var(--accent)' }}>Current speed:</b> {speedMultiplier}× &nbsp;·&nbsp;
          1 real second = {speedMultiplier} sim second{speedMultiplier > 1 ? 's' : ''}
        </div>
      </div>

      {/* ── Turbo / Fast-Forward ── */}
      <div className="card">
        <div className="section-title">⚡ Fast-Forward Simulation</div>
        <p style={{ fontSize: 12, color: 'var(--text-secondary)', marginBottom: 16 }}>
          Skip ahead by simulating multiple hours of orders and deliveries instantly.
          Perfect for generating rich analytics data without waiting.
        </p>

        <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 14 }}>
          <label style={{ fontSize: 12, color: 'var(--text-muted)', whiteSpace: 'nowrap' }}>Hours to simulate:</label>
          <input
            type="number"
            min={1} max={24}
            value={turboHours}
            onChange={e => setTurboHours(Number(e.target.value))}
            style={{
              background: 'var(--bg-glass)',
              border: '1px solid var(--border)',
              borderRadius: 6,
              padding: '6px 10px',
              color: 'var(--text-primary)',
              width: 70,
              fontFamily: 'var(--font)',
              fontSize: 14,
            }}
          />
          <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>
            ≈ {Math.round(turboHours * 3600 / 12)} orders
          </span>
        </div>

        <div style={{ display: 'flex', gap: 6, marginBottom: 12 }}>
          {[1, 2, 4, 8].map(h => (
            <button
              key={h}
              className={`btn ${turboHours === h ? 'btn-primary' : 'btn-ghost'}`}
              onClick={() => setTurboHours(h)}
              style={{ fontSize: 12 }}
            >
              {h}h
            </button>
          ))}
        </div>

        <button
          className="btn btn-warning"
          onClick={handleTurbo}
          disabled={turboLoading}
          style={{ width: '100%', justifyContent: 'center', padding: '10px' }}
        >
          {turboLoading ? '⏳ Simulating...' : `⚡ Fast-Forward ${turboHours} Hours`}
        </button>

        <div style={{
          marginTop: 12, fontSize: 11, color: 'var(--text-muted)',
          padding: '8px 12px', background: 'var(--bg-glass)',
          borderRadius: 6, border: '1px solid var(--border)'
        }}>
          ℹ️ Turbo mode generates all orders immediately, then assigns & processes them.
          Analytics will reflect the full {turboHours}-hour period.
        </div>
      </div>

      {/* ── Quick Stats ── */}
      {analytics && (
        <div className="card" style={{ gridColumn: 'span 2' }}>
          <div className="section-title">Quick Stats</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6,1fr)', gap: 12 }}>
            {[
              { label: 'Orders Placed',   value: analytics.total_orders_placed },
              { label: 'Delivered',        value: analytics.total_orders_delivered },
              { label: 'Batch Rate',       value: `${fmt(analytics.batch_rate_pct)}%` },
              { label: 'Dist Saved (km)', value: fmt(analytics.total_distance_saved_km) },
              { label: 'Fuel Saved (₹)',  value: `₹${fmt(analytics.total_fuel_saved_inr)}` },
              { label: 'Earnings Boost',  value: `+₹${fmt(analytics.total_earnings_increase)}` },
            ].map(k => (
              <div key={k.label} className="card-sm" style={{ textAlign: 'center' }}>
                <div className="kpi-label">{k.label}</div>
                <div style={{ fontSize: 20, fontWeight: 700, color: 'var(--text-primary)' }}>{k.value}</div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Event Log ── */}
      <div className="card" style={{ gridColumn: 'span 2' }}>
        <div className="section-title" style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Live Event Log</span>
          <span style={{ fontSize: 11, color: 'var(--text-muted)' }}>Last 50 events</span>
        </div>
        <div style={{
          height: 180,
          overflowY: 'auto',
          display: 'flex',
          flexDirection: 'column',
          gap: 4,
        }}>
          {eventLog.length === 0 && (
            <div style={{ color: 'var(--text-muted)', fontSize: 12, padding: 8 }}>
              No events yet. Start the simulation to see live events.
            </div>
          )}
          {eventLog.map((e, i) => (
            <div key={i} style={{
              fontSize: 12,
              color: 'var(--text-secondary)',
              padding: '4px 8px',
              borderRadius: 4,
              background: i === 0 ? 'rgba(99,102,241,0.06)' : 'transparent',
              borderLeft: i === 0 ? '2px solid var(--accent)' : '2px solid transparent',
            }}>
              {e.msg}
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}
