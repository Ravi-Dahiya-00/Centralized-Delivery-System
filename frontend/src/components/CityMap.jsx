import React, { useMemo, useState, useRef, useCallback, useEffect } from 'react'
import useStore from '../store/useStore'
import { useRiderAnimation } from '../hooks/useRiderAnimation'
import { getRushInfo } from '../store/useStore'

// Deterministic congestion: for a given edge + rush hour, decide if congested
// Uses a cheap hash so the same edges are always congested in the same hour
function isEdgeCongested(from, to, rushMult, simTimeSec) {
  if (rushMult < 1.2) return false  // only during rush hours
  const hour = Math.floor((simTimeSec % 86400) / 3600)
  const seed = (from * 31 + to * 17 + hour * 7) % 100
  // Congest ~25% of edges during mild rush, ~40% during heavy rush
  const threshold = rushMult >= 2.5 ? 40 : rushMult >= 1.5 ? 28 : 18
  return seed < threshold
}

// ── Canvas dimensions match the city coordinate space ──────────
const SVG_W = 2250
const SVG_H = 1560

// ── Visual constants ────────────────────────────────────────────
const ROAD_W   = 20   // road stroke width
const NODE_R   = 13   // restaurant / house radius
const INTER_R  = 7    // intersection dot radius
const RIDER_R  = 12   // rider circle radius

// Grid lines used to generate city blocks
const COLS = [300, 620, 940, 1260, 1580, 1900]
const ROWS = [180, 460, 740, 1020, 1300]

// ── Colors ─────────────────────────────────────────────────────
const NODE_COLORS = {
  INTERSECTION: { fill: '#52525b', stroke: '#71717a' },
  ZOMATO:       { fill: '#e23744', stroke: '#be2231', label: '#fca5a5' },
  SWIGGY:       { fill: '#fc8019', stroke: '#c96100', label: '#fed7aa' },
  BOTH:         { fill: '#7c3aed', stroke: '#5b21b6', label: '#c4b5fd' },
  HOUSE:        { fill: '#2563eb', stroke: '#1e40af', label: '#93c5fd' },
}

const RIDER_PALETTE = [
  '#6366f1', '#22c55e', '#f59e0b',
  '#ec4899', '#06b6d4', '#84cc16',
]
const ROUTE_PALETTE = [
  'rgba(99,102,241,0.8)',  'rgba(34,197,94,0.8)',
  'rgba(245,158,11,0.8)', 'rgba(236,72,153,0.8)',
  'rgba(6,182,212,0.8)',  'rgba(132,204,22,0.8)',
]

const ICONS = { ZOMATO: '🍕', SWIGGY: '🍔', BOTH: '🍽️', HOUSE: '🏠' }

// ── Deterministic tree positions (no randomness, stable renders) ─
function buildTrees() {
  const trees = []
  for (let ri = 0; ri < ROWS.length - 1; ri++) {
    for (let ci = 0; ci < COLS.length - 1; ci++) {
      const x1 = COLS[ci]   + ROAD_W + 12
      const x2 = COLS[ci+1] - ROAD_W - 12
      const y1 = ROWS[ri]   + ROAD_W + 12
      const y2 = ROWS[ri+1] - ROAD_W - 12
      const bw = x2 - x1, bh = y2 - y1
      const count = 6 + ((ri + ci) % 3)
      for (let t = 0; t < count; t++) {
        const h1 = (ri * 7  + ci * 13 + t * 31) % 97
        const h2 = (ri * 11 + ci * 17 + t * 23) % 97
        trees.push({ id: ri * 60 + ci * 10 + t, x: x1 + (bw * h1) / 97, y: y1 + (bh * h2) / 97 })
      }
    }
  }
  return trees
}
const TREES = buildTrees()

// ── Building block colors for city-block feel ───────────────────
const BLOCK_FILLS = [
  '#1e3a1e', '#1a3520', '#1f3d1a',
  '#1c3818', '#203c1c', '#1b3a1b',
]

export default function CityMap() {
  const { graph, riders, orders, selectedRiderId, setSelectedRider, simTimeSec, simState } = useStore()
  const { animPositions, trailPositions } = useRiderAnimation()
  const rush = (simState === 'RUNNING') ? getRushInfo(simTimeSec) : { mult: 0 }

  // ── Order pulse animations ──────────────────────────────────
  const [pulses, setPulses] = useState([])          // active ripple animations
  const prevOrderIdsRef = useRef(new Set())         // track which orders we've seen
  const pulseTimerRef = useRef(null)

  useEffect(() => {
    if (!graph || !orders.length) return
    const nodeMap = Object.fromEntries(graph.nodes.map(n => [n.id, n]))
    const newPulses = []
    orders.forEach(o => {
      if (!prevOrderIdsRef.current.has(o.id)) {
        prevOrderIdsRef.current.add(o.id)
        const node = nodeMap[o.restaurant_node]
        if (node) {
          newPulses.push({
            id: o.id, x: node.x, y: node.y,
            color: o.platform === 'ZOMATO' ? '#e23744' : '#fc8019',
            born: Date.now(),
          })
        }
      }
    })
    if (newPulses.length) {
      setPulses(prev => [...prev, ...newPulses].slice(-30)) // keep max 30
    }
  }, [orders, graph])

  // Clean up expired pulses every 500ms
  useEffect(() => {
    pulseTimerRef.current = setInterval(() => {
      const cutoff = Date.now() - 2200
      setPulses(prev => prev.filter(p => p.born > cutoff))
    }, 500)
    return () => clearInterval(pulseTimerRef.current)
  }, [])

  // ── Heatmap toggle ─────────────────────────────────────────
  const [showHeatmap, setShowHeatmap] = useState(false)

  // Count delivered orders per node
  const heatmapData = useMemo(() => {
    const counts = {}
    orders.forEach(o => {
      if (o.status === 'DELIVERED') {
        counts[o.customer_node] = (counts[o.customer_node] ?? 0) + 1
        counts[o.restaurant_node] = (counts[o.restaurant_node] ?? 0) + 0.5
      }
    })
    return counts
  }, [orders])

  const maxHeat = useMemo(() => Math.max(1, ...Object.values(heatmapData)), [heatmapData])

  // ── Pan / Zoom state ─────────────────────────────────────────
  const [pan,  setPan]  = useState({ x: 20, y: 20 })
  const [zoom, setZoom] = useState(0.52)
  const [dragging, setDragging] = useState(false)
  const dragOriginRef = useRef({ px: 0, py: 0, mx: 0, my: 0 })
  const containerRef = useRef(null)

  const onMouseDown = useCallback((e) => {
    if (e.button !== 0) return
    setDragging(true)
    dragOriginRef.current = { px: pan.x, py: pan.y, mx: e.clientX, my: e.clientY }
  }, [pan])

  const onMouseMove = useCallback((e) => {
    if (!dragging) return
    const { px, py, mx, my } = dragOriginRef.current
    setPan({ x: px + e.clientX - mx, y: py + e.clientY - my })
  }, [dragging])

  const onMouseUp = useCallback(() => setDragging(false), [])

  // Zoom toward cursor position
  const onWheel = useCallback((e) => {
    e.preventDefault()
    const rect = containerRef.current?.getBoundingClientRect()
    if (!rect) return
    const cx = e.clientX - rect.left
    const cy = e.clientY - rect.top
    const factor = e.deltaY < 0 ? 1.12 : 0.89
    const nz = Math.min(Math.max(zoom * factor, 0.2), 5)
    setPan(prev => ({
      x: cx - (cx - prev.x) * (nz / zoom),
      y: cy - (cy - prev.y) * (nz / zoom),
    }))
    setZoom(nz)
  }, [zoom])

  useEffect(() => {
    const el = containerRef.current
    if (!el) return
    el.addEventListener('wheel', onWheel, { passive: false })
    return () => el.removeEventListener('wheel', onWheel)
  }, [onWheel])

  // ── Derived data ─────────────────────────────────────────────
  const nodeMap = useMemo(
    () => graph ? Object.fromEntries(graph.nodes.map(n => [n.id, n])) : {},
    [graph]
  )

  const activeRoutes = useMemo(
    () => riders
      .filter(r => r.route?.length > 1)
      .map((r, i) => ({ riderId: r.id, route: r.route, color: ROUTE_PALETTE[i % ROUTE_PALETTE.length] })),
    [riders]
  )

  const pendingRestSet = useMemo(
    () => new Set(orders.filter(o => o.status === 'PENDING').map(o => o.restaurant_node)),
    [orders]
  )

  if (!graph) {
    return (
      <div style={{
        display: 'flex', flexDirection: 'column', alignItems: 'center',
        justifyContent: 'center', height: '100%',
        color: 'var(--text-muted)', gap: 10, fontSize: 14,
      }}>
        <div style={{ fontSize: 32 }}>🗺️</div>
        <div>Connecting to backend…</div>
        <div style={{ fontSize: 11, opacity: 0.5 }}>Make sure the C++ server is running on port 8081</div>
      </div>
    )
  }

  const selectedRider = riders.find(r => r.id === selectedRiderId)

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden', background: '#0d1f0d' }}>
      {/* ── Pannable / Zoomable Stage ──────────────────────── */}
      <div
        ref={containerRef}
        style={{ width: '100%', height: '100%', cursor: dragging ? 'grabbing' : 'grab', userSelect: 'none' }}
        onMouseDown={onMouseDown}
        onMouseMove={onMouseMove}
        onMouseUp={onMouseUp}
        onMouseLeave={onMouseUp}
      >
        <div style={{
          transform: `translate(${pan.x}px,${pan.y}px) scale(${zoom})`,
          transformOrigin: '0 0',
          willChange: 'transform',
        }}>
          <svg
            width={SVG_W} height={SVG_H}
            viewBox={`0 0 ${SVG_W} ${SVG_H}`}
            style={{ display: 'block', overflow: 'visible' }}
          >
            <defs>
              <filter id="glow-sm">
                <feGaussianBlur stdDeviation="3" result="b"/>
                <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
              </filter>
              <filter id="glow-lg">
                <feGaussianBlur stdDeviation="7" result="b"/>
                <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
              </filter>
              <filter id="drop-shadow">
                <feDropShadow dx="2" dy="2" stdDeviation="3" floodOpacity="0.5"/>
              </filter>
              {/* Road texture pattern */}
              <pattern id="road-dash" patternUnits="userSpaceOnUse" width="40" height="4">
                <rect width="40" height="4" fill="none"/>
                <rect x="0" y="1.5" width="20" height="1" fill="rgba(255,255,180,0.18)"/>
              </pattern>
            </defs>

            {/* ── 1. Background grass ──────────────────────── */}
            <rect width={SVG_W} height={SVG_H} fill="#112211"/>

            {/* ── 2. City blocks between roads ─────────────── */}
            {COLS.slice(0, -1).map((cx, ci) =>
              ROWS.slice(0, -1).map((cy, ri) => {
                const bx = cx + ROAD_W / 2 + 2
                const by = cy + ROAD_W / 2 + 2
                const bw = COLS[ci+1] - cx - ROAD_W - 4
                const bh = ROWS[ri+1] - cy - ROAD_W - 4
                return (
                  <rect key={`blk-${ci}-${ri}`}
                    x={bx} y={by} width={bw} height={bh}
                    fill={BLOCK_FILLS[(ci + ri * 3) % BLOCK_FILLS.length]}
                    rx={3}
                  />
                )
              })
            )}

            {/* ── 3. Trees ────────────────────────────────────── */}
            {TREES.map(t => (
              <g key={`tree-${t.id}`}>
                <circle cx={t.x} cy={t.y + 3} r={6}  fill="#0a1a0a" opacity={0.5}/>
                <circle cx={t.x} cy={t.y}     r={6}  fill="#1a4a1a" opacity={0.85}/>
                <circle cx={t.x} cy={t.y - 1} r={4}  fill="#236b23" opacity={0.9}/>
                <circle cx={t.x} cy={t.y - 2} r={2}  fill="#2d8f2d" opacity={0.85}/>
              </g>
            ))}

            {/* ── 4. Road surfaces ──────────────────────────── */}
            {graph.edges.map((e, i) => {
              const a = nodeMap[e.from], b = nodeMap[e.to]
              if (!a || !b) return null
              return (
                <line key={`road-${i}`}
                  x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                  stroke="#2e2e36" strokeWidth={ROAD_W} strokeLinecap="square"
                />
              )
            })}

            {/* Road edge borders (kerb lines) */}
            {graph.edges.map((e, i) => {
              const a = nodeMap[e.from], b = nodeMap[e.to]
              if (!a || !b) return null
              return (
                <g key={`kerb-${i}`}>
                  <line x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                    stroke="#3a3a45" strokeWidth={ROAD_W + 2} strokeLinecap="square"/>
                  <line x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                    stroke="#2e2e36" strokeWidth={ROAD_W} strokeLinecap="square"/>
                </g>
              )
            })}

            {/* Center dashes */}
            {graph.edges.map((e, i) => {
              const a = nodeMap[e.from], b = nodeMap[e.to]
              if (!a || !b) return null
              return (
                <line key={`dash-${i}`}
                  x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                  stroke="rgba(255,255,160,0.12)" strokeWidth={1.5}
                  strokeDasharray="18 14" strokeLinecap="round"
                />
              )
            })}

            {/* ── 5a. Congested road overlay (rush hours) ───── */}
            {graph.edges.map((e, i) => {
              const a = nodeMap[e.from], b = nodeMap[e.to]
              if (!a || !b) return null
              if (!isEdgeCongested(e.from, e.to, rush.mult, simTimeSec)) return null
              const isHeavy = rush.mult >= 2.5
              return (
                <g key={`cong-${i}`}>
                  <line x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                    stroke={isHeavy ? '#ef4444' : '#f97316'}
                    strokeWidth={ROAD_W - 2} strokeLinecap="square"
                    opacity={0.22} style={{ filter: 'url(#glow-sm)' }}
                  />
                  <line x1={a.x} y1={a.y} x2={b.x} y2={b.y}
                    stroke={isHeavy ? '#fca5a5' : '#fed7aa'}
                    strokeWidth={2} strokeLinecap="round"
                    strokeDasharray="8 12" opacity={0.55}
                  />
                </g>
              )
            })}

            {/* ── 5b. Active delivery routes ────────────────────── */}
            {activeRoutes.map((ar, i) => {
              const pts = ar.route
                .map(nid => nodeMap[nid]).filter(Boolean)
                .map(n => `${n.x},${n.y}`).join(' ')
              if (!pts) return null
              return (
                <g key={`route-${i}`}>
                  <polyline points={pts} fill="none"
                    stroke={ar.color} strokeWidth={10}
                    strokeLinecap="round" strokeLinejoin="round"
                    opacity={0.25}
                  />
                  <polyline points={pts} fill="none"
                    stroke={ar.color} strokeWidth={4}
                    strokeLinecap="round" strokeLinejoin="round"
                    style={{ filter: 'url(#glow-sm)' }}
                    opacity={0.9}
                  />
                </g>
              )
            })}

            {/* ── 5c. Heatmap overlay ───────────────────────────── */}
            {showHeatmap && graph.nodes.map(n => {
              const heat = heatmapData[n.id]
              if (!heat) return null
              const intensity = Math.min(heat / maxHeat, 1)
              const r = 28 + intensity * 52
              // gradient from cool-blue (low) -> yellow -> red (high)
              const hue = Math.round(240 - intensity * 240)
              const color = `hsl(${hue},90%,60%)`
              return (
                <circle key={`heat-${n.id}`}
                  cx={n.x} cy={n.y} r={r}
                  fill={color}
                  opacity={0.18 + intensity * 0.28}
                  style={{ filter: 'url(#glow-sm)', pointerEvents: 'none' }}
                />
              )
            })}

            {/* ── 6. Intersections ──────────────────────────── */}
            {graph.nodes
              .filter(n => n.type === 'INTERSECTION')
              .map(n => (
                <g key={`int-${n.id}`}>
                  <circle cx={n.x} cy={n.y} r={INTER_R + 2} fill="#252530"/>
                  <circle cx={n.x} cy={n.y} r={INTER_R}     fill="#3a3a48" stroke="#4a4a58" strokeWidth={1}/>
                </g>
              ))}

            {/* ── 7. Restaurants & Houses ───────────────────── */}
            {graph.nodes
              .filter(n => n.type !== 'INTERSECTION')
              .map(n => {
                const col = NODE_COLORS[n.type] ?? NODE_COLORS.HOUSE
                const isPending = pendingRestSet.has(n.id)
                const shortName = n.name.length > 13 ? n.name.slice(0, 12) + '…' : n.name
                return (
                  <g key={`node-${n.id}`}>
                    {/* Pending pulse ring */}
                    {isPending && (
                      <circle cx={n.x} cy={n.y} r={NODE_R + 9}
                        fill="none" stroke={col.fill} strokeWidth={2}
                        opacity={0.5}
                        style={{ animation: 'pulse-ring 1.4s ease-out infinite' }}
                      />
                    )}
                    {/* Drop shadow */}
                    <circle cx={n.x + 2} cy={n.y + 3} r={NODE_R + 3} fill="rgba(0,0,0,0.45)"/>
                    {/* Node circle */}
                    <circle cx={n.x} cy={n.y} r={NODE_R + 2}
                      fill={col.fill} stroke={col.stroke} strokeWidth={2.5}
                      style={{ filter: isPending ? 'url(#glow-sm)' : 'url(#drop-shadow)' }}
                    />
                    {/* Icon */}
                    <text x={n.x} y={n.y + 1}
                      fontSize={11} textAnchor="middle" dominantBaseline="middle">
                      {ICONS[n.type] ?? ''}
                    </text>
                    {/* Label */}
                    <text x={n.x} y={n.y + NODE_R + 12}
                      fontSize={8.5} fill={col.label ?? '#94a3b8'}
                      textAnchor="middle" dominantBaseline="middle"
                      style={{ fontFamily: 'Inter, sans-serif', fontWeight: 700, letterSpacing: 0.3 }}
                    >
                      {shortName}
                    </text>
                  </g>
                )
              })}

            {/* ── 8. Riders ─────────────────────────────────── */}
            {/* ── 7b. Rider trails ──────────────────────────────── */}
            {riders.map((r, i) => {
              const trail = trailPositions[r.id]
              if (!trail?.length) return null
              const color = RIDER_PALETTE[i % RIDER_PALETTE.length]
              return (
                <g key={`trail-${r.id}`}>
                  {trail.map((pt, ti) => {
                    const opacity = (1 - (ti + 1) / (trail.length + 1)) * 0.55
                    const radius  = RIDER_R * (1 - (ti / trail.length) * 0.6)
                    return (
                      <circle key={ti}
                        cx={pt.x} cy={pt.y} r={radius}
                        fill={color} opacity={opacity}
                      />
                    )
                  })}
                </g>
              )
            })}

            {/* ── 7c. Order spawn pulses ──────────────────────────── */}
            {pulses.map(p => {
              const age = (Date.now() - p.born) / 2200  // 0..1 over 2.2s
              const r1 = 18 + age * 55
              const r2 = 30 + age * 85
              const r3 = 10 + age * 35
              const op1 = Math.max(0, 0.7 - age * 0.7)
              const op2 = Math.max(0, 0.4 - age * 0.4)
              return (
                <g key={`pulse-${p.id}`} style={{ pointerEvents: 'none' }}>
                  <circle cx={p.x} cy={p.y} r={r1}
                    fill="none" stroke={p.color} strokeWidth={2.5} opacity={op1}/>
                  <circle cx={p.x} cy={p.y} r={r2}
                    fill="none" stroke={p.color} strokeWidth={1} opacity={op2}/>
                  <circle cx={p.x} cy={p.y} r={r3}
                    fill={p.color} opacity={op1 * 0.3}/>
                </g>
              )
            })}

            {/* ── 8. Riders ─────────────────────────────────────── */}
            {riders.map((r, i) => {
              const node = nodeMap[r.current_node]
              if (!node) return null
              // Use smooth RAF-interpolated position, fallback to node coords
              const anim = animPositions[r.id]
              const cx = anim?.x ?? node.x
              const cy = anim?.y ?? node.y
              const color = RIDER_PALETTE[i % RIDER_PALETTE.length]
              const isSelected = selectedRiderId === r.id
              const isMoving = r.status !== 'IDLE'
              const firstName = r.name?.split(' ')[0] ?? `R${r.id}`
              return (
                <g key={`rider-${r.id}`}
                  style={{ cursor: 'pointer' }}
                  onClick={e => { e.stopPropagation(); setSelectedRider(isSelected ? null : r.id) }}
                >
                  {/* Moving pulse */}
                  {isMoving && (
                    <circle cx={cx} cy={cy} r={RIDER_R + 8}
                      fill="none" stroke={color} strokeWidth={1.5}
                      opacity={0.35}
                      style={{ animation: 'pulse-ring 1.2s ease-out infinite' }}
                    />
                  )}
                  {/* Selection ring */}
                  {isSelected && (
                    <circle cx={cx} cy={cy} r={RIDER_R + 13}
                      fill="none" stroke={color} strokeWidth={2}
                      opacity={0.6}
                    />
                  )}
                  {/* Shadow */}
                  <circle cx={cx + 2} cy={cy + 3} r={RIDER_R + 1} fill="rgba(0,0,0,0.5)"/>
                  {/* Rider body */}
                  <circle cx={cx} cy={cy} r={RIDER_R}
                    fill={color}
                    stroke={isMoving ? 'rgba(255,255,255,0.85)' : 'rgba(255,255,255,0.3)'}
                    strokeWidth={isMoving ? 2 : 1}
                    opacity={isMoving ? 1 : 0.7}
                    style={{ filter: isMoving ? 'url(#glow-sm)' : undefined }}
                  />
                  <text x={cx} y={cy + 1.5} fontSize={10} textAnchor="middle" dominantBaseline="middle">🏍️</text>
                  {/* Rider name */}
                  <text x={cx} y={cy + RIDER_R + 12}
                    fontSize={8} fill={color}
                    textAnchor="middle" dominantBaseline="middle"
                    style={{ fontFamily: 'Inter, sans-serif', fontWeight: 800 }}
                  >
                    {firstName}
                  </text>
                </g>
              )
            })}
          </svg>
        </div>
      </div>

      {/* ── Zoom Controls ─────────────────────────────────────── */}
      <div style={{
        position: 'absolute', bottom: 86, right: 14,
        display: 'flex', flexDirection: 'column', gap: 5,
      }}>
        {[
          { label: '+', title: 'Zoom in',  action: () => setZoom(z => Math.min(z * 1.2, 5)) },
          { label: '−', title: 'Zoom out', action: () => setZoom(z => Math.max(z * 0.83, 0.2)) },
          { label: '⌂', title: 'Reset view', action: () => { setZoom(0.52); setPan({ x: 20, y: 20 }) } },
        ].map(btn => (
          <button key={btn.label} title={btn.title} onClick={btn.action}
            style={{
              width: 34, height: 34, borderRadius: 8, border: '1px solid rgba(255,255,255,0.1)',
              background: 'rgba(15,15,20,0.92)', color: 'white',
              fontSize: btn.label === '⌂' ? 14 : 18, cursor: 'pointer',
              backdropFilter: 'blur(8px)', display: 'flex',
              alignItems: 'center', justifyContent: 'center',
              transition: 'background 0.2s',
            }}
            onMouseEnter={e => e.currentTarget.style.background = 'rgba(99,102,241,0.4)'}
            onMouseLeave={e => e.currentTarget.style.background = 'rgba(15,15,20,0.92)'}
          >
            {btn.label}
          </button>
        ))}

        {/* Heatmap toggle */}
        <button
          title="Toggle delivery heatmap"
          onClick={() => setShowHeatmap(v => !v)}
          style={{
            width: 34, height: 34, borderRadius: 8, cursor: 'pointer',
            border: `1px solid ${showHeatmap ? '#f59e0b88' : 'rgba(255,255,255,0.1)'}`,
            background: showHeatmap ? 'rgba(245,158,11,0.25)' : 'rgba(15,15,20,0.92)',
            color: showHeatmap ? '#fcd34d' : 'white',
            fontSize: 16, backdropFilter: 'blur(8px)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            transition: 'all 0.2s',
          }}
        >🌡️</button>
      </div>

      {/* ── Zoom / pan hint ───────────────────────────────────── */}
      <div style={{
        position: 'absolute', bottom: 14, right: 14,
        background: 'rgba(10,12,18,0.88)', backdropFilter: 'blur(8px)',
        border: '1px solid rgba(255,255,255,0.07)',
        borderRadius: 6, padding: '4px 10px',
        fontSize: 10, color: 'rgba(255,255,255,0.4)', letterSpacing: 0.3,
      }}>
        {Math.round(zoom * 100)}% · Drag to pan · Scroll to zoom {rush.mult >= 1.2 ? `· ${rush.mult >= 2.5 ? 'HEAVY TRAFFIC' : 'TRAFFIC'}` : ''}
      </div>

      {/* ── Legend ────────────────────────────────────────────── */}
      <div style={{
        position: 'absolute', bottom: 14, left: 14,
        background: 'rgba(10,12,18,0.88)', backdropFilter: 'blur(8px)',
        border: '1px solid rgba(255,255,255,0.07)',
        borderRadius: 10, padding: '8px 14px',
        display: 'flex', gap: 14, fontSize: 11,
      }}>
        {[
          { icon: '🍕', label: 'Zomato',  color: '#e23744' },
          { icon: '🍔', label: 'Swiggy',  color: '#fc8019' },
          { icon: '🍽️', label: 'Both',   color: '#a78bfa' },
          { icon: '🏠', label: 'Home',    color: '#60a5fa' },
          { icon: '🏍️', label: 'Rider',  color: '#818cf8' },
        ].map(l => (
          <div key={l.label} style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
            <span style={{ fontSize: 13 }}>{l.icon}</span>
            <span style={{ color: l.color, fontWeight: 600 }}>{l.label}</span>
          </div>
        ))}
      </div>

      {/* ── Selected Rider Panel ───────────────────────────────── */}
      {selectedRider && (
        <div style={{
          position: 'absolute', top: 14, right: 14,
          background: 'rgba(9,9,11,0.95)', backdropFilter: 'blur(16px)',
          border: `1px solid ${RIDER_PALETTE[riders.indexOf(selectedRider) % RIDER_PALETTE.length]}44`,
          borderRadius: 14, padding: '16px 18px', minWidth: 230,
          boxShadow: '0 8px 40px rgba(0,0,0,0.5)',
        }}>
          <div style={{
            fontWeight: 800, fontSize: 15, color: '#f1f5f9', marginBottom: 12,
            display: 'flex', alignItems: 'center', gap: 8,
          }}>
            <span>🏍️</span>
            <span>{selectedRider.name}</span>
            <button onClick={() => setSelectedRider(null)}
              style={{
                marginLeft: 'auto', background: 'none', border: 'none',
                color: 'rgba(255,255,255,0.35)', cursor: 'pointer', fontSize: 16, lineHeight: 1,
              }}>✕</button>
          </div>
          {[
            ['Status',       selectedRider.status ?? '—'],
            ['Zone',         ['🌅 West','🏙️ Central','🌄 East'][selectedRider.zone_id ?? 0]],
            ['Delivered',    selectedRider.orders_delivered ?? 0],
            ['Distance',     `${((selectedRider.total_distance_km ?? 0)).toFixed(2)} km`],
            ['Earnings',     `₹${((selectedRider.earnings_inr ?? 0)).toFixed(2)}`],
            ['Earnings Boost',`+₹${((selectedRider.earnings_increase ?? 0)).toFixed(2)}`],
            ['Dist Saved',   `${((selectedRider.distance_saved_km ?? 0)).toFixed(2)} km`],
            ['Fuel Cost',    `₹${((selectedRider.fuel_cost_inr ?? 0)).toFixed(2)}`],
            ['Idle Ticks',   selectedRider.idle_ticks != null ? `${selectedRider.idle_ticks}/12` : '—'],
          ].map(([k, v]) => (
            <div key={k} style={{
              display: 'flex', justifyContent: 'space-between',
              padding: '5px 0', borderBottom: '1px solid rgba(255,255,255,0.05)',
              fontSize: 12,
            }}>
              <span style={{ color: 'rgba(255,255,255,0.45)' }}>{k}</span>
              <span style={{ color: '#f1f5f9', fontWeight: 600 }}>{v}</span>
            </div>
          ))}
        </div>
      )}

      {/* Inline keyframes for pulse animation */}
      <style>{`
        @keyframes pulse-ring {
          0%   { transform: scale(0.95); opacity: 0.7; }
          70%  { transform: scale(1.15); opacity: 0.2; }
          100% { transform: scale(0.95); opacity: 0; }
        }
      `}</style>
    </div>
  )
}
