import React from 'react'
import useStore, { getRushInfo } from './store/useStore'
import { useSimulation } from './hooks/useSimulation'
import CityMap from './components/CityMap'
import OrderTable from './components/OrderTable'
import AnalyticsDashboard from './components/AnalyticsDashboard'
import ControlPanel from './components/ControlPanel'

export default function App() {
  // Start polling
  useSimulation()

  const { activeTab, setActiveTab, simState, simTimeSec, speedMultiplier } = useStore()

  const isRunning = simState === 'RUNNING'
  const simHours = Math.floor(simTimeSec / 3600)
  const simMins  = Math.floor((simTimeSec % 3600) / 60)
  const simSecs  = simTimeSec % 60
  const simTimeStr = `${String(simHours).padStart(2,'0')}:${String(simMins).padStart(2,'0')}:${String(simSecs).padStart(2,'0')}`

  // Rush hour info — only meaningful when running
  const rush = isRunning ? getRushInfo(simTimeSec) : null

  const TABS = [
    { id: 'map',        label: '🗺️ Live Map'  },
    { id: 'orders',     label: '📋 Orders'    },
    { id: 'analytics',  label: '📊 Analytics' },
    { id: 'control',    label: '⚙️ Controls'  },
  ]

  return (
    <div style={{ height: '100vh', display: 'flex', flexDirection: 'column' }}>
      {/* ── Navbar ── */}
      <nav className="nav">
        <span className="nav-brand">🚀 CentralDelivery</span>

        {TABS.map(t => (
          <button
            key={t.id}
            className={`nav-tab ${activeTab === t.id ? 'active' : ''}`}
            onClick={() => setActiveTab(t.id)}
          >
            {t.label}
          </button>
        ))}

        <div className="sim-status">
          {/* Live indicator */}
          {isRunning && <span className="live-dot" />}
          <span style={{ color: isRunning ? 'var(--success)' : 'var(--text-muted)' }}>
            {isRunning ? 'LIVE' : simState}
          </span>

          {/* Sim clock */}
          <span className="sim-time">{simTimeStr}</span>

          {/* Rush hour pill — visible on every tab */}
          {rush && (
            <span style={{
              display: 'flex', alignItems: 'center', gap: 5,
              background: `${rush.color}18`,
              border: `1px solid ${rush.color}44`,
              color: rush.color,
              padding: '2px 8px', borderRadius: 20,
              fontSize: 11, fontWeight: 700,
              letterSpacing: 0.3,
              transition: 'all 0.6s ease',
            }}>
              {rush.emoji} {rush.phase}
              <span style={{ opacity: 0.7, fontWeight: 400 }}>{rush.mult}×</span>
            </span>
          )}

          {/* Speed multiplier */}
          {speedMultiplier !== 1 && (
            <span style={{
              background: 'rgba(245,158,11,0.12)',
              color: 'var(--warning)',
              padding: '2px 6px', borderRadius: 4,
              fontSize: 11, fontWeight: 600,
            }}>
              {speedMultiplier}×
            </span>
          )}

          <span style={{ color: 'var(--text-muted)', fontSize: 11 }}>
            Zomato + Swiggy · 6 Riders
          </span>
        </div>
      </nav>

      {/* ── Main Content ── */}
      <main style={{ flex: 1, overflow: 'hidden' }}>
        {activeTab === 'map' && (
          <div style={{ height: '100%', padding: 0 }}>
            <CityMap />
          </div>
        )}
        {activeTab === 'orders' && (
          <div className="page">
            <h1 style={{ fontSize: 18, fontWeight: 700, marginBottom: 16, color: 'var(--text-primary)' }}>
              Order Management
            </h1>
            <OrderTable />
          </div>
        )}
        {activeTab === 'analytics' && (
          <div className="page">
            <h1 style={{ fontSize: 18, fontWeight: 700, marginBottom: 16, color: 'var(--text-primary)' }}>
              Analytics Dashboard
            </h1>
            <AnalyticsDashboard />
          </div>
        )}
        {activeTab === 'control' && (
          <div className="page">
            <h1 style={{ fontSize: 18, fontWeight: 700, marginBottom: 16, color: 'var(--text-primary)' }}>
              Simulation Controls
            </h1>
            <ControlPanel />
          </div>
        )}
      </main>
    </div>
  )
}
