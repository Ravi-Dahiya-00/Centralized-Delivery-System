import React, { useState } from 'react'
import useStore from '../store/useStore'

const STATUS_CLASS = {
  PENDING:    'badge-pending',
  ASSIGNED:   'badge-assigned',
  PICKED_UP:  'badge-picked',
  DELIVERED:  'badge-delivered',
  CANCELLED:  'badge-cancelled',
}

export default function OrderTable() {
  const { orders, riders } = useStore()
  const [filter, setFilter] = useState('ALL')       // ALL | PENDING | ASSIGNED | PICKED_UP | DELIVERED
  const [platform, setPlatform] = useState('ALL')   // ALL | ZOMATO | SWIGGY

  const riderName = (id) => riders.find(r => r.id === id)?.name ?? '—'

  const filtered = orders.filter(o =>
    (filter   === 'ALL' || o.status   === filter) &&
    (platform === 'ALL' || o.platform === platform)
  ).slice().reverse()  // newest first

  const fmt = (sec) => {
    if (!sec || sec < 0) return '—'
    const m = Math.floor(sec / 60)
    const s = sec % 60
    return m > 0 ? `${m}m ${s}s` : `${s}s`
  }

  const STATUSES = ['ALL','PENDING','ASSIGNED','PICKED_UP','DELIVERED']

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>

      {/* ── Filters ── */}
      <div style={{ display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
        <div style={{ display: 'flex', gap: 4 }}>
          {STATUSES.map(s => (
            <button key={s} className={`btn ${filter === s ? 'btn-primary' : 'btn-ghost'}`}
              onClick={() => setFilter(s)} style={{ fontSize: 11 }}>
              {s === 'ALL' ? 'All' : s.replace('_', ' ')}
            </button>
          ))}
        </div>
        <div style={{ width: 1, height: 24, background: 'var(--border)' }} />
        <div style={{ display: 'flex', gap: 4 }}>
          {['ALL','ZOMATO','SWIGGY'].map(p => (
            <button key={p}
              className={`btn ${platform === p ? 'btn-primary' : 'btn-ghost'}`}
              onClick={() => setPlatform(p)} style={{ fontSize: 11 }}>
              {p === 'ZOMATO' ? '🍕 Zomato' : p === 'SWIGGY' ? '🍔 Swiggy' : 'All Platforms'}
            </button>
          ))}
        </div>
        <span style={{ marginLeft: 'auto', fontSize: 12, color: 'var(--text-muted)' }}>
          {filtered.length} orders
        </span>
      </div>

      {/* ── Table ── */}
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>#ID</th>
              <th>Platform</th>
              <th>From Restaurant</th>
              <th>To Customer</th>
              <th>Status</th>
              <th>Batch</th>
              <th>Rider</th>
              <th>Wait</th>
              <th>Delivery Time</th>
              <th>Solo Dist</th>
              <th>Actual Dist</th>
              <th>Saved</th>
            </tr>
          </thead>
          <tbody>
            {filtered.length === 0 && (
              <tr>
                <td colSpan={12} style={{ textAlign: 'center', color: 'var(--text-muted)', padding: 24 }}>
                  No orders yet. Start simulation to generate orders.
                </td>
              </tr>
            )}
            {filtered.map(o => (
              <tr key={o.id}>
                <td style={{ color: 'var(--text-primary)', fontWeight: 600 }}>#{o.id}</td>
                <td>
                  <span className={`badge badge-${o.platform.toLowerCase()}`}>
                    {o.platform === 'ZOMATO' ? '🍕' : '🍔'} {o.platform}
                  </span>
                </td>
                <td style={{ maxWidth: 140, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {o.restaurant_name}
                </td>
                <td style={{ maxWidth: 130, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {o.customer_name}
                </td>
                <td>
                  <span className={`badge ${STATUS_CLASS[o.status] ?? ''}`}>
                    {o.status}
                  </span>
                </td>
                <td style={{ color: o.batch_id > 0 ? 'var(--accent)' : 'var(--text-muted)' }}>
                  {o.batch_id > 0 ? `#${o.batch_id}` : '—'}
                </td>
                <td>{o.rider_id >= 0 ? riderName(o.rider_id) : '—'}</td>
                <td>{o.assigned_at > 0 ? fmt(o.assigned_at - o.placed_at) : '—'}</td>
                <td style={{ color: o.delivered_at > 0 ? 'var(--success)' : 'inherit' }}>
                  {o.delivered_at > 0 ? fmt(o.delivered_at - o.placed_at) : '—'}
                </td>
                <td style={{ fontSize: 12 }}>{o.solo_distance_m > 0 ? `${o.solo_distance_m}m` : '—'}</td>
                <td style={{ fontSize: 12 }}>{o.actual_distance_m > 0 ? `${o.actual_distance_m}m` : '—'}</td>
                <td style={{ color: o.distance_saved_m > 0 ? 'var(--success)' : 'var(--text-muted)', fontSize: 12 }}>
                  {o.distance_saved_m > 0 ? `↓${o.distance_saved_m}m` : '—'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  )
}
