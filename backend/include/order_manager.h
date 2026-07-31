#pragma once
#include "order.h"
#include "rider.h"
#include "graph.h"
#include "analytics.h"
#include <vector>
#include <queue>
#include <mutex>
#include <functional>

/*
 * OrderManager
 * ────────────
 * Manages the pending order queue and the batch formation logic.
 *
 * BATCHING RULES:
 *  1. Two orders are compatible if:
 *     a) Pickup distance ≤ MAX_BATCH_PICKUP_DISTANCE_M
 *     b) Extra travel distance added by batching ≤ EXTRA_DIST_THRESHOLD_PCT% of direct route
 *     c) Estimated total delivery time ≤ MAX_DELIVERY_TIME_SEC
 *  2. Max orders per batch: MAX_ORDERS_PER_BATCH (configurable, default 3)
 *  3. Orders older than MAX_WAIT_BEFORE_SOLO_SEC are sent solo regardless
 */

struct BatchProposal {
    std::vector<int> order_ids;
    int              total_estimated_distance_m;
    int              solo_total_distance_m;
    double           efficiency_score;  // higher = better (savings ratio)
};

class OrderManager {
public:
    OrderManager(const Graph& g,
                 int max_orders_per_batch,
                 int max_pickup_dist_m,
                 int max_delivery_time_sec,
                 double extra_dist_threshold_pct,
                 int max_wait_before_solo_sec);

    // Add a new order to the pending queue
    void enqueue(const Order& o);

    // Called every simulation tick — returns ready batches for assignment
    // A "ready batch" = either: 2 compatible orders found, OR
    //                           1 order waited too long (solo dispatch)
    std::vector<BatchProposal> processPending(long long current_sim_sec);

    // Mark order as assigned (remove from pending)
    void markAssigned(int order_id);

    // BUG-1 FIX: clear all orders and reset pending queue (called on sim reset)
    void resetOrders() {
        orders.clear();
        pending_ids.clear();
        next_order_id = 1;
    }

    // Access all orders (for API)
    const std::vector<Order>& allOrders() const { return orders; }
    std::vector<Order>& allOrders() { return orders; }

    Order* findOrder(int id);
    const Order* findOrder(int id) const;

    // Next order ID
    int nextOrderId() { return next_order_id++; }

    bool hasPendingOrders() const { return !pending_ids.empty(); }

private:
    const Graph&  g;
    int max_orders_per_batch;
    int max_pickup_dist_m;
    int max_delivery_time_sec;
    double extra_dist_threshold_pct;
    int max_wait_sec;
    int next_order_id = 1;

    std::vector<Order>     orders;        // ALL orders ever placed
    std::vector<int>       pending_ids;   // IDs of PENDING orders

    // Check if two orders can be batched together
    bool areCompatible(const Order& a, const Order& b, long long sim_sec) const;

    // Estimate extra distance if two orders are batched
    int estimateBatchDistance(const Order& a, const Order& b) const;

    // Estimate extra distance if three orders are batched
    int estimateTripleDistance(const Order& a, const Order& b, const Order& c) const;
};
