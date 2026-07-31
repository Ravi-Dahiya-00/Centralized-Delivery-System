import { useEffect, useRef } from 'react'
import { api } from '../api/api'
import useStore from '../store/useStore'

/*
 * useSimulation — polling hook
 * Fetches /api/graph once on mount, then polls /api/state every 500ms.
 * All state is written to the Zustand store.
 */
export function useSimulation() {
  const { setGraph, setLiveState, addEvent } = useStore()
  const prevOrders = useRef([])

  // Load graph once
  useEffect(() => {
    api.getGraph()
      .then(g => setGraph(g))
      .catch(err => console.error('[API] Failed to load graph:', err))
  }, [])

  // Poll live state
  useEffect(() => {
    const interval = setInterval(async () => {
      try {
        const data = await api.getState()
        setLiveState(data)

        // Detect new delivered orders and log events
        const newDelivered = data.orders.filter(o =>
          o.status === 'DELIVERED' &&
          !prevOrders.current.find(p => p.id === o.id && p.status === 'DELIVERED')
        )
        newDelivered.forEach(o => {
          addEvent(`✅ Order #${o.id} (${o.platform}) delivered to ${o.customer_name}`)
        })

        const newPicked = data.orders.filter(o =>
          o.status === 'PICKED_UP' &&
          !prevOrders.current.find(p => p.id === o.id && p.status === 'PICKED_UP')
        )
        newPicked.forEach(o => {
          addEvent(`🥡 Order #${o.id} picked up from ${o.restaurant_name}`)
        })

        const newAssigned = data.orders.filter(o =>
          o.status === 'ASSIGNED' &&
          !prevOrders.current.find(p => p.id === o.id && (p.status === 'ASSIGNED' || p.status === 'PICKED_UP' || p.status === 'DELIVERED'))
        )
        newAssigned.forEach(o => {
          addEvent(`🏍️ Rider heading to ${o.restaurant_name} for order #${o.id}`)
        })

        const newOrders = data.orders.filter(o =>
          !prevOrders.current.find(p => p.id === o.id)
        )
        newOrders.forEach(o => {
          addEvent(`📦 New ${o.platform} order #${o.id} from ${o.restaurant_name}`)
        })

        prevOrders.current = data.orders
        if (!data.orders?.length) prevOrders.current = []
      } catch {
        // Backend not started yet — silently retry
      }
    }, 500)

    return () => clearInterval(interval)
  }, [])
}
