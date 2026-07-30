import { useRef, useEffect, useState } from 'react'
import useStore from '../store/useStore'

const TICK_MS    = 780   // slightly under backend tick for crisp arrivals
const TRAIL_LEN  = 8     // number of past positions to keep per rider

function easeInOut(t) {
  return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t
}

/**
 * useRiderAnimation
 * ─────────────────
 * Returns:
 *   animPositions  — { [riderId]: { x, y } }  smooth interpolated current pos
 *   trailPositions — { [riderId]: [{x,y}, ...] } last TRAIL_LEN visited coords
 *                    index 0 = most recent past position (fades last)
 */
export function useRiderAnimation() {
  const riders = useStore(s => s.riders)
  const graph  = useStore(s => s.graph)

  const [animPositions,  setAnimPositions]  = useState({})
  const [trailPositions, setTrailPositions] = useState({})

  const animRef  = useRef({})  // { riderId: { fromX,fromY,toX,toY,startTime } }
  const prevRef  = useRef({})  // { riderId: { node, x, y } }
  const trailRef = useRef({})  // { riderId: [{x,y},...] }
  const rafRef   = useRef(null)

  // ── When API pushes new rider state ──────────────────────────
  useEffect(() => {
    if (!graph || !riders.length) return
    const nodeMap = Object.fromEntries(graph.nodes.map(n => [n.id, n]))
    const now = performance.now()

    riders.forEach(rider => {
      const node = nodeMap[rider.current_node]
      if (!node) return
      const toX = node.x, toY = node.y
      const prev = prevRef.current[rider.id]

      if (!prev) {
        // First time — snap to position
        prevRef.current[rider.id]  = { node: rider.current_node, x: toX, y: toY }
        animRef.current[rider.id]  = { fromX: toX, fromY: toY, toX, toY, startTime: now - TICK_MS }
        trailRef.current[rider.id] = []
      } else if (prev.node !== rider.current_node) {
        // Rider moved — record previous position into trail
        const fromX = animRef.current[rider.id]?.toX ?? prev.x
        const fromY = animRef.current[rider.id]?.toY ?? prev.y

        // Push old position into trail (newest at front)
        const newTrail = [{ x: fromX, y: fromY }, ...(trailRef.current[rider.id] ?? [])]
          .slice(0, TRAIL_LEN)
        trailRef.current[rider.id] = newTrail

        animRef.current[rider.id] = { fromX, fromY, toX, toY, startTime: now }
        prevRef.current[rider.id] = { node: rider.current_node, x: toX, y: toY }
      }
    })
  }, [riders, graph])

  // ── RAF loop — runs forever while mounted ─────────────────────
  useEffect(() => {
    const loop = () => {
      const now  = performance.now()
      const pos  = {}
      const trail = {}

      Object.entries(animRef.current).forEach(([riderId, anim]) => {
        const raw = Math.min((now - anim.startTime) / TICK_MS, 1)
        const t   = easeInOut(raw)
        pos[riderId] = {
          x: anim.fromX + (anim.toX - anim.fromX) * t,
          y: anim.fromY + (anim.toY - anim.fromY) * t,
        }
        trail[riderId] = trailRef.current[riderId] ?? []
      })

      setAnimPositions(pos)
      setTrailPositions(trail)
      rafRef.current = requestAnimationFrame(loop)
    }

    rafRef.current = requestAnimationFrame(loop)
    return () => cancelAnimationFrame(rafRef.current)
  }, [])

  return { animPositions, trailPositions }
}
