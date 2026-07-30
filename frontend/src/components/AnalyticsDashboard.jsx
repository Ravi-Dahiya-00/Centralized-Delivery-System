import React, { useMemo } from 'react'
import {
  LineChart, Line, BarChart, Bar, PieChart, Pie, Cell,
  XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Legend
} from 'recharts'
import useStore, { getRushInfo } from '../store/useStore'

const RIDER_COLORS = ['#6366f1', '#22c55e', '#f59e0b', '#ec4899', '#06b6d4', '#84cc16']

// ── CSV Export ───────────────────────────────────────────────
function exportCSV(analytics, orders, simTimeSec) {
  if (!analytics) return
  const a = analytics
  const rs = a.rider_stats ?? []
  const rush = getRushInfo(simTimeSec)

  const rows = []

  // ── Section 1: System Summary ────────────────────────────
  rows.push(['=== CENTRALIZED DELIVERY SYSTEM - ANALYTICS REPORT ==='])
  rows.push([`Generated at Sim Time: ${rush.label} (${rush.phase})`])
  rows.push([])
  rows.push(['--- SYSTEM SUMMARY ---'])
  rows.push(['Metric', 'Value'])
  rows.push(['Total Orders Placed',          a.total_orders_placed])
  rows.push(['Total Orders Delivered',        a.total_orders_delivered])
  rows.push(['Total Batched Orders',          a.total_batched_orders])
  rows.push(['Total Solo Orders',             a.total_solo_orders])
  rows.push(['Batch Rate (%)',                Number(a.batch_rate_pct).toFixed(2)])
  rows.push(['Orders Per Hour (system)',      Number(a.system_orders_per_hour).toFixed(2)])
  rows.push([''])
  rows.push(['Total Distance Traveled (km)',  Number(a.total_distance_km).toFixed(3)])
  rows.push(['Solo Baseline Distance (km)',   Number(a.total_solo_distance_km).toFixed(3)])
  rows.push(['Total Distance Saved (km)',     Number(a.total_distance_saved_km).toFixed(3)])
  rows.push(['Avg Distance Saved (%)',        Number(a.avg_distance_saved_pct).toFixed(2)])
  rows.push([''])
  rows.push(['Total Actual Earnings (INR)',   Number(a.total_earnings_inr).toFixed(2)])
  rows.push(['Solo Baseline Earnings (INR)',  Number(a.total_solo_earnings_inr).toFixed(2)])
  rows.push(['Total Earnings Increase (INR)', Number(a.total_earnings_increase).toFixed(2)])
  rows.push(['Earnings Increase (%)',         Number(a.earnings_increase_pct).toFixed(2)])
  rows.push(['Total Fuel Cost (INR)',         Number(a.total_fuel_cost_inr).toFixed(2)])
  rows.push(['Total Fuel Saved (INR)',        Number(a.total_fuel_saved_inr).toFixed(2)])
  rows.push([])

  // ── Section 2: Per-Rider Breakdown ───────────────────────
  rows.push(['--- PER-RIDER BREAKDOWN ---'])
  rows.push([
    'Rider Name', 'Zone', 'Orders Delivered', 'Batches',
    'Actual Distance (km)', 'Solo Distance (km)', 'Distance Saved (km)',
    'Avg Saved/Order (km)',
    'Actual Earnings (INR)', 'Solo Earnings (INR)', 'Earnings Increase (INR)', 'Earnings Increase (%)',
    'Fuel Cost (INR)', 'Fuel Saved (INR)', 'Orders/Hour'
  ])
  const ZONES = ['West', 'Central', 'East']
  rs.forEach(r => {
    const avgSaved = r.orders_delivered > 0
      ? (r.distance_saved_km / r.orders_delivered).toFixed(3) : '0'
    rows.push([
      r.rider_name,
      ZONES[r.zone_id ?? 0] ?? 'West',
      r.orders_delivered,
      r.batches_completed ?? 0,
      Number(r.actual_distance_km).toFixed(3),
      Number(r.solo_distance_km).toFixed(3),
      Number(r.distance_saved_km).toFixed(3),
      avgSaved,
      Number(r.earnings_inr).toFixed(2),
      Number(r.solo_earnings_inr).toFixed(2),
      Number(r.earnings_increase).toFixed(2),
      Number(r.earnings_increase_pct).toFixed(2),
      Number(r.fuel_cost_inr).toFixed(2),
      Number(r.fuel_saved_inr).toFixed(2),
      Number(r.orders_per_hour).toFixed(2),
    ])
  })
  rows.push([])

  // ── Section 3: Order Log ───────────────────────────────
  if (orders?.length) {
    rows.push(['--- ORDER LOG ---'])
    rows.push([
      'Order ID', 'Platform', 'Restaurant', 'Customer',
      'Status', 'Batch ID', 'Actual Dist (m)', 'Solo Dist (m)', 'Dist Saved (m)'
    ])
    orders.forEach(o => {
      rows.push([
        o.id, o.platform,
        o.restaurant_name ?? o.restaurant_node,
        o.customer_name ?? o.customer_node,
        o.status, o.batch_id ?? 'solo',
        o.actual_distance_m ?? 0,
        o.solo_distance_m ?? 0,
        o.distance_saved_m ?? 0,
      ])
    })
  }

  // ── Build and download file ────────────────────────────
  const csv = rows.map(r =>
    r.map(v => (typeof v === 'string' && v.includes(',')) ? `"${v}"` : v).join(',')
  ).join('\n')

  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
  const url  = URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href     = url
  link.download = `delivery_analytics_${rush.label.replace(':','h')}m.csv`
  link.click()
  URL.revokeObjectURL(url)
}

// ── Helpers ────────────────────────────────────────────────────
const fmt    = (n, dec = 1) => (n != null ? Number(n).toFixed(dec) : '0')
const fmtINR = (n) => `₹${fmt(n, 2)}`
const fmtKm  = (n) => `${fmt(n, 2)} km`
const fmtPct = (n) => `${fmt(n, 1)}%`
const simHHMM = (sec) => {
  const h = Math.floor(sec / 3600)
  const m = Math.floor((sec % 3600) / 60)
  return `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}`
}

// ── KPI Card ────────────────────────────────────────────────────
function KPI({ label, value, sub, color }) {
  return (
    <div className="kpi-card">
      <div className="kpi-label">{label}</div>
      <div className="kpi-value" style={color ? { color } : {}}>{value ?? '—'}</div>
      {sub && <div className="kpi-sub">{sub}</div>}
    </div>
  )
}

// ── Mini SVG Sparkline ───────────────────────────────────────────
function Sparkline({ data, color = '#6366f1', height = 44 }) {
  if (!data || data.length < 2) return <div style={{ height }} />
  const min = Math.min(...data)
  const max = Math.max(...data)
  const range = max - min || 1
  const w = 180, h = height
  const pts = data.map((v, i) => {
    const x = (i / (data.length - 1)) * w
    const y = h - ((v - min) / range) * (h - 4) - 2
    return `${x},${y}`
  }).join(' ')
  return (
    <svg width={w} height={h} style={{ overflow: 'visible' }}>
      <defs>
        <linearGradient id={`sg-${color.replace('#','')}`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity={0.3}/>
          <stop offset="100%" stopColor={color} stopOpacity={0}/>
        </linearGradient>
      </defs>
      <polyline points={pts} fill="none" stroke={color} strokeWidth={2}
        strokeLinecap="round" strokeLinejoin="round"/>
      <polyline
        points={`0,${h} ${pts} ${w},${h}`}
        fill={`url(#sg-${color.replace('#','')})`} stroke="none"/>
    </svg>
  )
}

// ── Rush Hour Banner ────────────────────────────────────────────
function RushBanner({ simTimeSec }) {
  const info = getRushInfo(simTimeSec)
  const barW = Math.min((info.mult / 3.0) * 100, 100)
  return (
    <div style={{
      background: 'rgba(24,24,27,0.7)', border: `1px solid ${info.color}44`,
      borderRadius: 12, padding: '14px 20px',
      display: 'flex', alignItems: 'center', gap: 18,
      backdropFilter: 'blur(8px)',
    }}>
      <span style={{ fontSize: 28 }}>{info.emoji}</span>
      <div style={{ flex: 1 }}>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 8, marginBottom: 6 }}>
          <span style={{ fontWeight: 800, fontSize: 16, color: info.color }}>{info.phase}</span>
          <span style={{ fontSize: 11, color: 'rgba(255,255,255,0.4)' }}>
            Sim time {info.label} · {info.mult}× demand
          </span>
        </div>
        <div style={{ height: 6, background: 'rgba(255,255,255,0.06)', borderRadius: 3, overflow: 'hidden' }}>
          <div style={{
            height: '100%', width: `${barW}%`, borderRadius: 3,
            background: `linear-gradient(90deg, ${info.color}88, ${info.color})`,
            transition: 'width 0.8s ease',
          }}/>
        </div>
      </div>
      <div style={{ textAlign: 'right' }}>
        <div style={{ fontSize: 11, color: 'rgba(255,255,255,0.35)' }}>Order rate</div>
        <div style={{ fontWeight: 700, fontSize: 15, color: info.color }}>{info.mult}×</div>
      </div>
    </div>
  )
}

// ── Live Line Chart for a single metric over time ───────────────
function LiveLineChart({ history, dataKey, color, label, formatter }) {
  const data = history.map(h => ({
    t: simHHMM(h.t),
    v: h[dataKey] ?? 0,
  }))
  return (
    <div className="card" style={{ padding: '16px 12px' }}>
      <div className="section-title" style={{ marginBottom: 8 }}>{label}</div>
      <ResponsiveContainer width="100%" height={160}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="2 6" stroke="rgba(255,255,255,0.04)"/>
          <XAxis dataKey="t" tick={{ fill: '#475569', fontSize: 9 }} interval="preserveStartEnd"/>
          <YAxis tick={{ fill: '#475569', fontSize: 9 }} width={48}
            tickFormatter={formatter ?? (v => v.toFixed(1))}/>
          <Tooltip
            contentStyle={{ background: '#18181b', border: '1px solid #27272a', borderRadius: 8, fontSize: 11 }}
            formatter={(v) => [formatter ? formatter(v) : v.toFixed(2), label]}
            labelFormatter={(l) => `Sim ${l}`}
          />
          <Line type="monotone" dataKey="v" stroke={color} strokeWidth={2}
            dot={false} activeDot={{ r: 4, fill: color }}/>
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}

// ── Per-Rider Earnings over time ─────────────────────────────────
function RiderEarningsChart({ history }) {
  if (!history.length) return null
  // Build data array: each point has riderId -> earnings value
  const data = history.map(h => {
    const pt = { t: simHHMM(h.t) }
    h.riderEarnings?.forEach(r => { pt[r.name.split(' ')[0]] = parseFloat(r.v.toFixed(2)) })
    return pt
  })
  const riderNames = history[history.length - 1]?.riderEarnings?.map(r => r.name.split(' ')[0]) ?? []

  return (
    <div className="card" style={{ padding: '16px 12px' }}>
      <div className="section-title" style={{ marginBottom: 8 }}>📈 Per-Rider Earnings Over Time (₹)</div>
      <ResponsiveContainer width="100%" height={180}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="2 6" stroke="rgba(255,255,255,0.04)"/>
          <XAxis dataKey="t" tick={{ fill: '#475569', fontSize: 9 }} interval="preserveStartEnd"/>
          <YAxis tick={{ fill: '#475569', fontSize: 9 }} width={48} tickFormatter={v => `₹${v}`}/>
          <Tooltip
            contentStyle={{ background: '#18181b', border: '1px solid #27272a', borderRadius: 8, fontSize: 11 }}
            formatter={(v, name) => [`₹${v.toFixed(2)}`, name]}
            labelFormatter={(l) => `Sim ${l}`}
          />
          <Legend wrapperStyle={{ fontSize: 10, color: '#94a3b8' }}/>
          {riderNames.map((name, i) => (
            <Line key={name} type="monotone" dataKey={name}
              stroke={RIDER_COLORS[i % RIDER_COLORS.length]}
              strokeWidth={1.8} dot={false}
              activeDot={{ r: 3 }}/>
          ))}
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}

// ── Main Dashboard ───────────────────────────────────────────────
export default function AnalyticsDashboard() {
  const { analytics, riders, simTimeSec, analyticsHistory, simState } = useStore()

  const isRunning = simState === 'RUNNING'

  if (!analytics) {
    return (
      <div style={{ color: 'var(--text-muted)', padding: 40, textAlign: 'center' }}>
        <div style={{ fontSize: 40, marginBottom: 12 }}>📊</div>
        No analytics data yet. Start the simulation first.
      </div>
    )
  }

  const a  = analytics
  const rs = a.rider_stats ?? []
  const hoursElapsed = (simTimeSec / 3600).toFixed(2)

  const pieData = [
    { name: 'Batched', value: a.total_batched_orders ?? 0,  color: '#6366f1' },
    { name: 'Solo',    value: a.total_solo_orders   ?? 0,  color: '#475569' },
  ]

  const riderEarningsData = rs.map(r => ({
    name:         r.rider_name.split(' ')[0],
    Actual:       Number(fmt(r.earnings_inr, 2)),
    SoloBaseline: Number(fmt(r.solo_earnings_inr, 2)),
    Boost:        Number(fmt(r.earnings_increase, 2)),
  }))

  const riderDistData = rs.map(r => ({
    name:   r.rider_name.split(' ')[0],
    Actual: Number(fmt(r.actual_distance_km, 2)),
    Solo:   Number(fmt(r.solo_distance_km, 2)),
    Saved:  Number(fmt(r.distance_saved_km, 2)),
  }))

  // Sparkline source arrays
  const sparkEarnings  = analyticsHistory.map(h => h.earnings)
  const sparkDistSaved = analyticsHistory.map(h => h.distSaved)
  const sparkBatchRate = analyticsHistory.map(h => h.batchRate)

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 20 }}>

      {/* ── Top bar: title + export ── */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ fontSize: 13, color: 'var(--text-muted)' }}>
          Live analytics · updates every 500ms
        </div>
        <button
          onClick={() => exportCSV(analytics, useStore.getState().orders, simTimeSec)}
          style={{
            display: 'flex', alignItems: 'center', gap: 7,
            background: 'linear-gradient(135deg,#6366f1,#4f46e5)',
            border: 'none', borderRadius: 8,
            padding: '8px 16px', cursor: 'pointer',
            color: 'white', fontWeight: 700, fontSize: 12,
            boxShadow: '0 2px 12px rgba(99,102,241,0.4)',
            transition: 'transform 0.15s, box-shadow 0.15s',
          }}
          onMouseEnter={e => { e.currentTarget.style.transform = 'translateY(-1px)'; e.currentTarget.style.boxShadow = '0 4px 20px rgba(99,102,241,0.55)' }}
          onMouseLeave={e => { e.currentTarget.style.transform = ''; e.currentTarget.style.boxShadow = '0 2px 12px rgba(99,102,241,0.4)' }}
        >
          <span>⬇️</span> Export CSV Report
        </button>
      </div>

      {/* ── Rush Hour Banner ── */}
      <RushBanner simTimeSec={simTimeSec}/>

      {/* ── System Overview KPIs ── */}
      <div>
        <div className="section-title">System Overview</div>
        <div className="kpi-grid">
          <KPI label="Orders Placed"     value={a.total_orders_placed}/>
          <KPI label="Orders Delivered"  value={a.total_orders_delivered}/>
          <KPI label="Batch Rate"        value={fmtPct(a.batch_rate_pct)} color="var(--accent)"/>
          <KPI label="Orders / Hour"     value={fmt(a.system_orders_per_hour)}/>
          <KPI label="Sim Hours Elapsed" value={hoursElapsed} sub="virtual hours"/>
        </div>
      </div>

      {/* ── Earnings KPIs + sparkline ── */}
      <div>
        <div className="section-title">💰 Earnings Impact (Batching vs. Solo Baseline)</div>
        <div style={{ display: 'flex', gap: 12, alignItems: 'stretch' }}>
          <div className="kpi-grid" style={{ flex: 1 }}>
            <KPI label="Total Actual Earnings"  value={fmtINR(a.total_earnings_inr)}  color="var(--success)"/>
            <KPI label="Solo Baseline Earnings" value={fmtINR(a.total_solo_earnings_inr)} color="var(--text-muted)"/>
            <KPI label="Earnings Increased By"  value={fmtINR(a.total_earnings_increase)} color="var(--success)"
                 sub={`+${fmtPct(a.earnings_increase_pct)} boost`}/>
          </div>
          <div style={{
            background: 'rgba(24,24,27,0.5)', border: '1px solid rgba(255,255,255,0.05)',
            borderRadius: 12, padding: '12px 16px', display: 'flex',
            flexDirection: 'column', gap: 6, justifyContent: 'center',
          }}>
            <div style={{ fontSize: 10, color: '#475569', marginBottom: 2 }}>Total Earnings Trend</div>
            <Sparkline data={sparkEarnings} color="#22c55e" height={44}/>
          </div>
        </div>
      </div>

      {/* ── Distance & Fuel KPIs + sparkline ── */}
      <div>
        <div className="section-title">📏 Distance & Fuel Savings</div>
        <div style={{ display: 'flex', gap: 12, alignItems: 'stretch' }}>
          <div className="kpi-grid" style={{ flex: 1 }}>
            <KPI label="Total Distance Traveled" value={fmtKm(a.total_distance_km)}/>
            <KPI label="Solo Baseline Distance"  value={fmtKm(a.total_solo_distance_km)} color="var(--text-muted)"/>
            <KPI label="Total Distance Saved"    value={fmtKm(a.total_distance_saved_km)} color="var(--success)"
                 sub={`${fmtPct(a.avg_distance_saved_pct)} reduction`}/>
            <KPI label="Fuel Cost Actual"        value={fmtINR(a.total_fuel_cost_inr)}/>
            <KPI label="Fuel Cost Saved"         value={fmtINR(a.total_fuel_saved_inr)} color="var(--success)"
                 sub="vs. solo model"/>
          </div>
          <div style={{
            background: 'rgba(24,24,27,0.5)', border: '1px solid rgba(255,255,255,0.05)',
            borderRadius: 12, padding: '12px 16px', display: 'flex',
            flexDirection: 'column', gap: 6, justifyContent: 'center',
          }}>
            <div style={{ fontSize: 10, color: '#475569', marginBottom: 2 }}>Distance Saved Trend</div>
            <Sparkline data={sparkDistSaved} color="#6366f1" height={44}/>
          </div>
        </div>
      </div>

      {/* ── Live Charts row ── */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 14 }}>
        <LiveLineChart
          history={analyticsHistory}
          dataKey="earnings"
          color="#22c55e"
          label="💰 Total Earnings Over Time (₹)"
          formatter={v => `₹${v.toFixed(0)}`}
        />
        <LiveLineChart
          history={analyticsHistory}
          dataKey="distSaved"
          color="#6366f1"
          label="📏 Distance Saved Over Time (km)"
          formatter={v => `${v.toFixed(2)}km`}
        />
        <LiveLineChart
          history={analyticsHistory}
          dataKey="batchRate"
          color="#f59e0b"
          label="🎯 Batch Rate Over Time (%)"
          formatter={v => `${v.toFixed(1)}%`}
        />
        <LiveLineChart
          history={analyticsHistory}
          dataKey="ordersPlaced"
          color="#ec4899"
          label="📦 Total Orders Over Time"
          formatter={v => Math.round(v)}
        />
      </div>

      {/* ── Per-Rider Earnings over time ── */}
      <RiderEarningsChart history={analyticsHistory}/>

      {/* ── Static Charts row ── */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
        <div className="card">
          <div className="section-title">Orders: Batched vs Solo</div>
          <ResponsiveContainer width="100%" height={220}>
            <PieChart>
              <Pie data={pieData} cx="50%" cy="50%"
                innerRadius={60} outerRadius={90} paddingAngle={3} dataKey="value"
                label={({ name, value }) => `${name}: ${value}`} labelLine={false}>
                {pieData.map((entry, i) => <Cell key={i} fill={entry.color}/>)}
              </Pie>
              <Tooltip contentStyle={{ background: '#18181b', border: '1px solid #27272a', borderRadius: 8 }}/>
              <Legend wrapperStyle={{ fontSize: 12, color: '#94a3b8' }}/>
            </PieChart>
          </ResponsiveContainer>
        </div>

        <div className="card">
          <div className="section-title">Per-Rider Earnings (₹)</div>
          <ResponsiveContainer width="100%" height={220}>
            <BarChart data={riderEarningsData}>
              <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)"/>
              <XAxis dataKey="name" tick={{ fill: '#475569', fontSize: 11 }}/>
              <YAxis tick={{ fill: '#475569', fontSize: 11 }}/>
              <Tooltip contentStyle={{ background: '#18181b', border: '1px solid #27272a', borderRadius: 8 }}
                formatter={(v, name) => [`₹${v.toFixed(2)}`, name]}/>
              <Legend wrapperStyle={{ fontSize: 11, color: '#94a3b8' }}/>
              <Bar dataKey="Actual"       fill="#6366f1" radius={[4,4,0,0]}/>
              <Bar dataKey="SoloBaseline" fill="#475569" radius={[4,4,0,0]}/>
            </BarChart>
          </ResponsiveContainer>
        </div>
      </div>

      {/* ── Distance Chart ── */}
      <div className="card">
        <div className="section-title">Per-Rider Distance: Actual vs Solo Baseline (km)</div>
        <ResponsiveContainer width="100%" height={200}>
          <BarChart data={riderDistData}>
            <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)"/>
            <XAxis dataKey="name" tick={{ fill: '#475569', fontSize: 11 }}/>
            <YAxis tick={{ fill: '#475569', fontSize: 11 }}/>
            <Tooltip contentStyle={{ background: '#18181b', border: '1px solid #27272a', borderRadius: 8 }}
              formatter={(v, name) => [`${v.toFixed(2)} km`, name]}/>
            <Legend wrapperStyle={{ fontSize: 11, color: '#94a3b8' }}/>
            <Bar dataKey="Actual" fill="#6366f1" radius={[4,4,0,0]}/>
            <Bar dataKey="Solo"   fill="#475569" radius={[4,4,0,0]}/>
            <Bar dataKey="Saved"  fill="#22c55e" radius={[4,4,0,0]}/>
          </BarChart>
        </ResponsiveContainer>
      </div>

      {/* ── Per-Rider Detailed Table ── */}
      <div className="card">
        <div className="section-title">👤 Per-Rider Detailed Breakdown</div>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Rider</th><th>Orders</th><th>Actual Dist</th><th>Solo Dist</th>
                <th>Dist Saved</th><th>Avg Saved/Order</th><th>Earnings</th>
                <th>Solo Earnings</th><th>Earnings ↑</th><th>Earnings ↑ %</th>
                <th>Fuel Cost</th><th>Fuel Saved</th><th>Orders/hr</th>
              </tr>
            </thead>
            <tbody>
              {rs.length === 0 && (
                <tr><td colSpan={13} style={{ textAlign: 'center', color: '#475569', padding: 20 }}>No data yet.</td></tr>
              )}
              {rs.map((r, i) => (
                <tr key={r.rider_id}>
                  <td style={{ color: RIDER_COLORS[i % RIDER_COLORS.length], fontWeight: 600 }}>{r.rider_name}</td>
                  <td>{r.orders_delivered}</td>
                  <td>{fmtKm(r.actual_distance_km)}</td>
                  <td style={{ color: '#475569' }}>{fmtKm(r.solo_distance_km)}</td>
                  <td style={{ color: '#22c55e' }}>↓{fmtKm(r.distance_saved_km)}</td>
                  <td style={{ color: '#22c55e', fontSize: 11 }}>
                    {r.orders_delivered > 0 ? fmtKm(r.distance_saved_km / r.orders_delivered) + '/order' : '—'}
                  </td>
                  <td style={{ color: '#22c55e' }}>{fmtINR(r.earnings_inr)}</td>
                  <td style={{ color: '#475569' }}>{fmtINR(r.solo_earnings_inr)}</td>
                  <td style={{ color: '#22c55e', fontWeight: 600 }}>+{fmtINR(r.earnings_increase)}</td>
                  <td style={{ color: '#22c55e' }}>+{fmtPct(r.earnings_increase_pct)}</td>
                  <td>{fmtINR(r.fuel_cost_inr)}</td>
                  <td style={{ color: '#22c55e' }}>↓{fmtINR(r.fuel_saved_inr)}</td>
                  <td>{fmt(r.orders_per_hour)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  )
}
