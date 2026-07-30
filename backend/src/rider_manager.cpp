#include "../include/rider_manager.h"
#include <iostream>
#include <algorithm>
#include <climits>
#include <unordered_map>


// How many idle ticks before rider returns to their zone center
static constexpr int IDLE_RETURN_TICKS = 12;

RiderManager::RiderManager(const Graph& g,
                           OrderManager& om,
                           double base_pay_inr,
                           double dist_bonus_per_km,
                           double fuel_cost_per_km)
    : g(g), om(om), optimizer(g),
      base_pay_inr(base_pay_inr),
      dist_bonus_per_km(dist_bonus_per_km),
      fuel_cost_per_km(fuel_cost_per_km) {}

void RiderManager::addRider(const Rider& r) {
    riders_.push_back(r);
}

// ─── BUG-1 FIX: Reset — free all in-flight batches, reset stats ─────
void RiderManager::reset() {
    for (auto& rider : riders_) {
        if (rider.batch) {
            delete rider.batch;
            rider.batch = nullptr;
        }
        // Reset all runtime state but keep identity/zone
        rider.status            = RiderStatus::IDLE;
        rider.idle_ticks        = 0;
        rider.orders_delivered  = 0;
        rider.batches_completed = 0;
        rider.total_distance_m  = 0.0;
        rider.solo_distance_m   = 0.0;
        rider.earnings_inr      = 0.0;
        rider.solo_earnings_inr = 0.0;
        rider.fuel_cost_inr     = 0.0;
        rider.solo_fuel_cost_inr= 0.0;
        // Restore to zone center if zones are assigned
        if (rider.zone_node >= 0) {
            rider.current_node = rider.zone_node;
            rider.x = g.nodes[rider.zone_node].x;
            rider.y = g.nodes[rider.zone_node].y;
        }
    }
    next_batch_id = 1;
}

// ─── Zone Assignment ───────────────────────────────────────
// Divides city width into 3 equal zones (West/Central/East).
// Each rider is assigned a zone based on their start node x coord.
// Then we find the nearest INTERSECTION in that zone as their home base.
void RiderManager::assignZones() {
    // Find city x bounds
    double min_x = 1e9, max_x = -1e9;
    for (const auto& n : g.nodes) { min_x = std::min(min_x, n.x); max_x = std::max(max_x, n.x); }
    double zone_w = (max_x - min_x) / 3.0;

    for (auto& rider : riders_) {
        // Determine zone from starting node
        const auto& start = g.nodes[rider.current_node];
        int zone = (start.x < min_x + zone_w) ? 0
                 : (start.x < min_x + 2 * zone_w) ? 1 : 2;
        rider.zone_id = zone;

        // BUG-11 FIX: zone boundary is exclusive for left, inclusive for right
        // Zone 0: [min_x, min_x+zone_w)
        // Zone 1: [min_x+zone_w, min_x+2*zone_w)
        // Zone 2: [min_x+2*zone_w, max_x]
        double zone_lo = min_x + zone * zone_w;
        double zone_hi = (zone < 2) ? (zone_lo + zone_w) : max_x + 1.0;

        int best_node = rider.current_node;
        int best_dist = INT_MAX;
        for (const auto& n : g.nodes) {
            if (n.type != NodeType::INTERSECTION) continue;
            // BUG-11 FIX: exclusive upper bound so no node is in two zones
            if (n.x < zone_lo || n.x >= zone_hi) continue;
            int d = g.distance(rider.current_node, n.id);
            if (d < best_dist) { best_dist = d; best_node = n.id; }
        }
        rider.zone_node = best_node;
        std::cout << "[Zone] Rider '" << rider.name << "' → Zone "
                  << zone << " (home node " << rider.zone_node << ")\n";
    }
}


// ─── Find nearest idle rider to a given node ─────────────────
int RiderManager::findNearestIdle(int target_node) const {
    int best_idx  = -1;
    int best_dist = INT_MAX;
    for (int i = 0; i < (int)riders_.size(); ++i) {
        if (riders_[i].status != RiderStatus::IDLE) continue;
        int d = g.distance(riders_[i].current_node, target_node);
        if (d < best_dist) {
            best_dist = d;
            best_idx  = i;
        }
    }
    return best_idx;
}

// ─── Assign a batch to the nearest idle rider ─────────────────
bool RiderManager::assignBatch(const BatchProposal& proposal,
                               long long sim_sec,
                               int& out_batch_id) {
    if (proposal.order_ids.empty()) return false;

    // Use first order's restaurant as the "first pickup" location
    Order* first = om.findOrder(proposal.order_ids[0]);
    if (!first) return false;

    int rider_idx = findNearestIdle(first->restaurant_node);
    if (rider_idx == -1) {
        std::cout << "[Assign] No idle rider available for batch.\n";
        return false;
    }

    Rider& rider = riders_[rider_idx];

    // Build stops list for the optimizer
    std::vector<Stop> stops;
    for (int oid : proposal.order_ids) {
        Order* o = om.findOrder(oid);
        if (!o) continue;
        stops.push_back({o->restaurant_node, oid, true,  false});  // pickup
        stops.push_back({o->customer_node,   oid, false, false});  // delivery
    }

    // Get optimized route
    OptimizedRoute route = optimizer.optimize(rider.current_node, stops);

    // BUG-5 FIX: guard against empty or unreachable route
    if (route.full_path.empty() || route.total_distance == INT_MAX) {
        std::cout << "[Assign] Route unreachable for batch — skipped.\n";
        return false;
    }

    // Build the ActiveBatch
    ActiveBatch* batch = new ActiveBatch();
    batch->batch_id        = next_batch_id++;
    batch->order_ids       = proposal.order_ids;
    batch->full_path       = route.full_path;
    batch->total_distance_m = route.total_distance;
    batch->solo_distance_m  = proposal.solo_total_distance_m;
    batch->stops           = stops;
    batch->path_index      = 0;
    batch->stops_done      = 0;

    out_batch_id = batch->batch_id;

    rider.batch  = batch;
    rider.status = RiderStatus::MOVING_TO_PICKUP;

    // Mark all orders as ASSIGNED and set metadata
    for (int oid : proposal.order_ids) {
        Order* o = om.findOrder(oid);
        if (!o) continue;
        o->status             = OrderStatus::ASSIGNED;
        o->batch_id           = batch->batch_id;
        o->rider_id           = rider.id;
        o->assigned_at_sim_sec = sim_sec;
        o->solo_distance_m    = g.distance(o->restaurant_node, o->customer_node);
        om.markAssigned(oid);
    }

    std::cout << "[Assign] Batch #" << batch->batch_id
              << " → Rider '" << rider.name << "'"
              << " | Orders: " << proposal.order_ids.size()
              << " | Path nodes: " << route.full_path.size()
              << " | Est dist: " << route.total_distance << "m\n";
    std::cout << "  Route: " << route.description << "\n";

    return true;
}

// ─── Tick: advance all active riders one step ─────────────
void RiderManager::tick(long long sim_sec) {
    for (auto& rider : riders_) {
        // ── IDLE riders: track idle time → maybe return to zone ─────
        if (rider.status == RiderStatus::IDLE && !rider.batch) {
            rider.idle_ticks++;
            if (rider.idle_ticks >= IDLE_RETURN_TICKS && rider.zone_node >= 0
                && rider.current_node != rider.zone_node) {
                returnToZone(rider);
            }
            continue;
        }

        if (!rider.batch) continue;
        ActiveBatch* batch = rider.batch;

        // Advance one step in path
        if (batch->path_index + 1 < (int)batch->full_path.size()) {
            batch->path_index++;
            int prev_node = rider.current_node;
            rider.current_node = batch->full_path[batch->path_index];

            // Track actual distance — use precomputed shortest dist for adjacent nodes
            int edge_dist = g.distance(prev_node, rider.current_node);
            if (edge_dist != Graph::INF) {
                rider.total_distance_m += edge_dist;
                rider.fuel_cost_inr    += (edge_dist / 1000.0) * fuel_cost_per_km;
            }

            // Update visual position from node coords
            rider.x = g.nodes[rider.current_node].x;
            rider.y = g.nodes[rider.current_node].y;

            // Check if we've arrived at a stop
            processArrival(rider, sim_sec);
        } else {
            // Path complete — all stops done
            bool is_repositioning = rider.batch->order_ids.empty();
            if (!is_repositioning) {
                rider.batches_completed++;
                std::cout << "[Done] Rider '" << rider.name
                          << "' completed batch #" << batch->batch_id << "\n";
            } else {
                std::cout << "[Zone] Rider '" << rider.name
                          << "' arrived at zone center (node " << rider.current_node << ")\n";
            }
            rider.status = RiderStatus::IDLE;
            rider.idle_ticks = 0;
            delete rider.batch;
            rider.batch = nullptr;
        }
    }
}


// ─── BUG-4 FIX: processArrival uses done flag + exact matching ──────
// Matches each stop by (order_id, is_pickup) AND checks done flag
// so the same stop is never processed twice (even at shared nodes).
void RiderManager::processArrival(Rider& rider, long long sim_sec) {
    if (!rider.batch) return;
    ActiveBatch* batch = rider.batch;
    int cur = rider.current_node;

    for (auto& stop : batch->stops) {
        if (stop.done)          continue;   // already processed
        if (stop.node_id != cur) continue;   // not here yet

        Order* o = om.findOrder(stop.order_id);
        if (!o) { stop.done = true; continue; }

        if (stop.is_pickup && o->status == OrderStatus::ASSIGNED) {
            o->status              = OrderStatus::PICKED_UP;
            o->picked_up_at_sim_sec = sim_sec;
            stop.done              = true;
            // Only update to MOVING_TO_DELIVERY if we still have deliveries pending
            rider.status = RiderStatus::MOVING_TO_DELIVERY;
            std::cout << "[Pickup] Rider '" << rider.name
                      << "' picked up Order #" << o->id
                      << " at node " << cur << "\n";

        } else if (!stop.is_pickup && o->status == OrderStatus::PICKED_UP) {
            // BUG-12 FIX: only process delivery after pickup is confirmed done
            o->status               = OrderStatus::DELIVERED;
            o->delivered_at_sim_sec = sim_sec;
            stop.done               = true;
            bool is_solo = (batch->order_ids.size() == 1);
            finalizeOrder(rider, *o, is_solo, sim_sec);
            batch->stops_done++;
            std::cout << "[Delivered] Order #" << o->id
                      << " → node " << cur
                      << " | Time: " << (o->deliveryTime() / 60) << "min\n";
        }
    }
}

// ─── Return idle rider to their zone center ─────────────────
void RiderManager::returnToZone(Rider& rider) {
    int target = rider.zone_node;
    auto path = g.getPath(rider.current_node, target);
    if (path.size() < 2) return;  // already at zone center

    ActiveBatch* repo = new ActiveBatch();
    repo->batch_id        = next_batch_id++;
    repo->order_ids       = {};   // empty = repositioning move
    repo->full_path       = path;
    repo->total_distance_m = 0;
    repo->solo_distance_m  = 0;
    repo->path_index      = 0;
    repo->stops_done      = 0;

    rider.batch      = repo;
    rider.status     = RiderStatus::MOVING_TO_PICKUP;
    rider.idle_ticks = 0;

    std::cout << "[Zone] Rider '" << rider.name
              << "' repositioning to zone " << rider.zone_id
              << " center (node " << target << ")\n";
}


// ─── BUG-7 FIX: per-order distance uses solo_distance_m directly ────
// Instead of splitting batch total equally, each order records its own
// direct restaurant→customer Dijkstra distance as actual_distance_m.
// The total_distance_m on the rider still accumulates real edges traveled.
void RiderManager::finalizeOrder(Rider& rider, Order& order,
                                 bool is_solo, long long /*sim_sec*/) {
    rider.orders_delivered++;

    // Per-order distance: the direct Dijkstra dist (already set in assignBatch)
    double my_dist_m  = (double)order.solo_distance_m;
    double my_dist_km = my_dist_m / 1000.0;
    double solo_km    = my_dist_km;  // same baseline for solo

    order.actual_distance_m = (int)my_dist_m;

    // ── Actual earnings for this delivery ──────────────────
    // Batching lets rider do more deliveries per trip — base pay per order + dist bonus
    double earned = base_pay_inr + dist_bonus_per_km * my_dist_km;
    rider.earnings_inr += earned;
    // NOTE: fuel_cost_inr is accumulated per-edge in tick() — not added here

    // ── Solo baseline: what would have been earned if no batching ──
    double solo_earned = base_pay_inr + dist_bonus_per_km * solo_km;
    rider.solo_earnings_inr  += solo_earned;
    rider.solo_distance_m    += order.solo_distance_m;
    rider.solo_fuel_cost_inr += solo_km * fuel_cost_per_km;
    // BUG-9 FIX: removed dead `if (is_solo) rider.solo_earnings_inr += 0;`
    (void)is_solo;  // used for future per-order metadata if needed
}

// ─── Build analytics snapshot ─────────────────────────────────
SystemAnalytics RiderManager::buildAnalytics(long long sim_sec,
                                              long long real_elapsed_ms,
                                              int total_placed,
                                              int total_cancelled) const {
    SystemAnalytics sa;
    sa.simulation_time_sec    = sim_sec;
    sa.real_time_elapsed_ms   = real_elapsed_ms;
    sa.total_orders_placed    = total_placed;
    sa.total_cancelled        = total_cancelled;

    for (const auto& rider : riders_) {
        RiderStats rs;
        rs.rider_id   = rider.id;
        rs.rider_name = rider.name;
        rs.orders_delivered   = rider.orders_delivered;
        rs.batches_completed  = rider.batches_completed;

        rs.actual_distance_km = rider.total_distance_m / 1000.0;
        rs.solo_distance_km   = rider.solo_distance_m  / 1000.0;
        // BUG: distance_saved can go negative if actual > solo (rider did detours)
        // Clamp to 0 to avoid confusing negative values in UI
        double raw_saved = (rider.solo_distance_m - rider.total_distance_m) / 1000.0;
        rs.distance_saved_km  = std::max(0.0, raw_saved);

        rs.earnings_inr       = rider.earnings_inr;
        rs.solo_earnings_inr  = rider.solo_earnings_inr;
        rs.earnings_increase  = rider.earnings_inr - rider.solo_earnings_inr;
        rs.earnings_increase_pct = (rider.solo_earnings_inr > 0)
            ? (rs.earnings_increase / rider.solo_earnings_inr) * 100.0
            : 0.0;

        rs.fuel_cost_inr      = rider.fuel_cost_inr;
        rs.solo_fuel_cost_inr = rider.solo_fuel_cost_inr;
        rs.fuel_saved_inr     = rider.solo_fuel_cost_inr - rider.fuel_cost_inr;

        double hours = sim_sec / 3600.0;
        rs.orders_per_hour = (hours > 0) ? rider.orders_delivered / hours : 0.0;

        sa.rider_stats.push_back(rs);

        // Accumulate system totals
        sa.total_orders_delivered    += rider.orders_delivered;
        sa.total_distance_km         += rs.actual_distance_km;
        sa.total_solo_distance_km    += rs.solo_distance_km;
        sa.total_distance_saved_km   += rs.distance_saved_km;
        sa.total_earnings_inr        += rs.earnings_inr;
        sa.total_solo_earnings_inr   += rs.solo_earnings_inr;
        sa.total_fuel_cost_inr       += rs.fuel_cost_inr;
        sa.total_fuel_saved_inr      += rs.fuel_saved_inr;
    }

    // BUG-8 FIX: compute batch_rate_pct from actual order data
    {
        int batched_count = 0;
        int solo_count    = 0;
        for (const auto& o : om.allOrders()) {
            if (o.status != OrderStatus::DELIVERED) continue;
            // Solo = batch of 1 order. We detect by checking if batch_id exists
            // but we count all delivered orders — those in batches of >1 are "batched"
            // Use solo_distance_m == actual_distance_m as a proxy (set equal for solo)
            if (o.batch_id >= 0) {
                // Count orders in their batch — we approximate:
                // any order with batch_id that was batched with another = batched
                // For now track: delivered alone vs delivered in multi-order batch
                // We check by counting how many orders share the same batch_id
                batched_count++;  // will be refined below
            } else {
                solo_count++;
            }
        }
        // Better: count orders sharing a batch_id > 1 using a frequency map
        std::unordered_map<int, int> batch_freq;
        for (const auto& o : om.allOrders()) {
            if (o.status == OrderStatus::DELIVERED && o.batch_id >= 0)
                batch_freq[o.batch_id]++;
        }
        batched_count = 0; solo_count = 0;
        for (const auto& o : om.allOrders()) {
            if (o.status != OrderStatus::DELIVERED) continue;
            if (o.batch_id < 0 || batch_freq[o.batch_id] == 1)
                solo_count++;
            else
                batched_count++;
        }
        sa.total_batched_orders = batched_count;
        sa.total_solo_orders    = solo_count;
        if (sa.total_orders_delivered > 0)
            sa.batch_rate_pct = 100.0 * batched_count / sa.total_orders_delivered;
    }

    // System-level derived metrics
    sa.total_earnings_increase = sa.total_earnings_inr - sa.total_solo_earnings_inr;
    sa.earnings_increase_pct   = (sa.total_solo_earnings_inr > 0)
        ? (sa.total_earnings_increase / sa.total_solo_earnings_inr) * 100.0
        : 0.0;

    if (sa.total_solo_distance_km > 0)
        sa.avg_distance_saved_pct = (sa.total_distance_saved_km / sa.total_solo_distance_km) * 100.0;

    double hours = sim_sec / 3600.0;
    sa.system_orders_per_hour = (hours > 0)
        ? sa.total_orders_delivered / hours : 0.0;

    return sa;
}
