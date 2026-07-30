#pragma once
#include "rider.h"
#include "order.h"
#include "order_manager.h"
#include "route_optimizer.h"
#include "analytics.h"
#include "graph.h"
#include <vector>
#include <mutex>

/*
 * RiderManager
 * ────────────
 * Handles rider assignment and movement simulation.
 *
 * ASSIGNMENT ALGORITHM (Greedy Nearest):
 *  For each batch proposal:
 *   1. Find all IDLE riders
 *   2. Compute distance from each idle rider to the first pickup node
 *   3. Assign to the nearest idle rider
 *   4. Compute optimal multi-stop route via RouteOptimizer
 *
 * MOVEMENT SIMULATION:
 *  Each tick, each active rider advances one step along their route path.
 *  Speed is simulated as: advance 1 edge per tick.
 *  When a rider reaches a pickup node → mark order PICKED_UP
 *  When a rider reaches a delivery node → mark order DELIVERED, update analytics
 */

class RiderManager {
public:
    RiderManager(const Graph& g,
                 OrderManager& om,
                 double base_pay_inr,
                 double dist_bonus_per_km,
                 double fuel_cost_per_km);

    void addRider(const Rider& r);

    // Assign a batch proposal to the nearest idle rider
    // Returns true if assignment was successful
    bool assignBatch(const BatchProposal& proposal, long long sim_sec, int& out_batch_id);

    // Called every simulation tick — advances riders along their routes
    void tick(long long sim_sec);

    // Assign home zones to all riders based on starting node position
    void assignZones();

    // BUG-1 fix: Release all in-flight batch memory and reset rider state
    void reset();

    // Build analytics snapshot from current rider state
    // BUG-8 fix: takes OrderManager so it can count batched/solo properly
    SystemAnalytics buildAnalytics(long long sim_sec, long long real_elapsed_ms,
                                   int total_placed, int total_cancelled) const;

    std::vector<Rider>& riders() { return riders_; }
    const std::vector<Rider>& riders() const { return riders_; }

private:
    const Graph&    g;
    OrderManager&   om;
    RouteOptimizer  optimizer;

    double base_pay_inr;
    double dist_bonus_per_km;
    double fuel_cost_per_km;

    std::vector<Rider> riders_;
    int next_batch_id = 1;

    // Find nearest idle rider to a given node; returns rider index or -1
    int findNearestIdle(int target_node) const;

    // Process stops when rider arrives at a node
    void processArrival(Rider& rider, long long sim_sec);

    // Finalize a delivered order — update analytics on the rider
    void finalizeOrder(Rider& rider, Order& order, bool is_solo, long long sim_sec);

    // Send idle rider back toward their zone center
    void returnToZone(Rider& rider);
};

