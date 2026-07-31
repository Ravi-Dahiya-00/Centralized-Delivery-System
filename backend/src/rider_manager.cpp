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
        rider.delivery_distance_m = 0.0;
        rider.solo_distance_m   = 0.0;
        rider.earnings_inr      = 0.0;
        rider.solo_earnings_inr = 0.0;
        rider.fuel_cost_inr     = 0.0;
        rider.solo_fuel_cost_inr= 0.0;
        rider.route_distance_saved_m = 0.0;
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

bool RiderManager::hasActiveRiders() const {
    for (const auto& r : riders_) {
        if (r.batch) return true;
    }
    return false;
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

    // Fair solo baseline: deliver each order alone, sequentially, from the same start
    // (rider → rest → cust → next rest → …). Comparable to the batched multi-stop route.
    int fair_solo = 0;
    int solo_pos  = rider.current_node;
    for (int oid : proposal.order_ids) {
        Order* o = om.findOrder(oid);
        if (!o) continue;
        int d1 = g.distance(solo_pos, o->restaurant_node);
        int d2 = g.distance(o->restaurant_node, o->customer_node);
        if (d1 == Graph::INF || d2 == Graph::INF) {
            std::cout << "[Assign] Solo baseline unreachable — skipped.\n";
            return false;
        }
        fair_solo += d1 + d2;
        solo_pos = o->customer_node;
    }

    // Build the ActiveBatch
    ActiveBatch* batch = new ActiveBatch();
    batch->batch_id         = next_batch_id++;
    batch->order_ids        = proposal.order_ids;
    batch->full_path        = route.full_path;
    batch->total_distance_m = route.total_distance;
    batch->solo_distance_m  = fair_solo;
    batch->stops            = stops;
    batch->path_index       = 0;
    batch->stops_done       = 0;
    batch->stats_applied    = false;

    out_batch_id = batch->batch_id;

    rider.batch  = batch;
    rider.status = RiderStatus::MOVING_TO_PICKUP;

    // Mark all orders as ASSIGNED and set metadata
    int n = (int)proposal.order_ids.size();
    for (int oid : proposal.order_ids) {
        Order* o = om.findOrder(oid);
        if (!o) continue;
        o->status              = OrderStatus::ASSIGNED;
        o->batch_id            = batch->batch_id;
        o->rider_id            = rider.id;
        o->assigned_at_sim_sec = sim_sec;
        // Solo trip if this order alone from rider's current position
        int approach = g.distance(rider.current_node, o->restaurant_node);
        int direct   = g.distance(o->restaurant_node, o->customer_node);
        o->solo_distance_m = (approach == Graph::INF || direct == Graph::INF)
                           ? 0 : approach + direct;
        // Share of batched route attributed to this order (set precisely on finalize)
        o->actual_distance_m = (n > 0) ? route.total_distance / n : 0;
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
                // Delivery batches only (skip empty zone-reposition trips)
                if (!batch->order_ids.empty()) {
                    rider.delivery_distance_m += edge_dist;
                    rider.fuel_cost_inr += (edge_dist / 1000.0) * fuel_cost_per_km;
                }
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
                applyBatchStats(rider, *batch);
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


// ─── Per-order earnings; batch-level savings applied in applyBatchStats ────
void RiderManager::finalizeOrder(Rider& rider, Order& order,
                                 bool is_solo, long long /*sim_sec*/) {
    rider.orders_delivered++;

    ActiveBatch* batch = rider.batch;
    int n = (batch && !batch->order_ids.empty())
          ? (int)batch->order_ids.size() : 1;
    int batch_actual = (batch) ? batch->total_distance_m : order.solo_distance_m;
    order.actual_distance_m = batch_actual / std::max(n, 1);

    // Pay is based on restaurant→customer distance (order value), not route share
    int rest_cust = g.distance(order.restaurant_node, order.customer_node);
    if (rest_cust == Graph::INF) rest_cust = 0;
    double rest_cust_km = rest_cust / 1000.0;
    double earned = base_pay_inr + dist_bonus_per_km * rest_cust_km;
    rider.earnings_inr += earned;

    (void)is_solo;
}

// ─── Batch-level distance saved + capacity-adjusted solo earnings ─────────
void RiderManager::applyBatchStats(Rider& rider, ActiveBatch& batch) {
    if (batch.stats_applied || batch.order_ids.empty()) return;
    batch.stats_applied = true;

    double actual_m = (double)batch.total_distance_m;
    double solo_m   = (double)batch.solo_distance_m;
    if (solo_m < actual_m) solo_m = actual_m;  // never credit negative "savings"

    rider.solo_distance_m += solo_m;

    double saved_m = solo_m - actual_m;
    rider.route_distance_saved_m += saved_m;
    rider.solo_fuel_cost_inr += (solo_m / 1000.0) * fuel_cost_per_km;

    // Capacity model: without batching, the same delivery-km would finish fewer orders.
    // ratio = actual/solo ≤ 1 → solo baseline earnings for this trip's work effort.
    double batch_pay = 0.0;
    for (int oid : batch.order_ids) {
        Order* o = om.findOrder(oid);
        if (!o) continue;
        int rest_cust = g.distance(o->restaurant_node, o->customer_node);
        if (rest_cust == Graph::INF) rest_cust = 0;
        batch_pay += base_pay_inr + dist_bonus_per_km * (rest_cust / 1000.0);
    }
    double ratio = (solo_m > 0.0) ? (actual_m / solo_m) : 1.0;
    if (ratio > 1.0) ratio = 1.0;
    rider.solo_earnings_inr += batch_pay * ratio;
}

// ─── Build analytics snapshot ─────────────────────────────────
SystemAnalytics RiderManager::buildAnalytics(long long sim_sec,
                                              long long real_elapsed_ms,
                                              int total_placed,
                                              int /*total_cancelled*/) const {
    SystemAnalytics sa;
    sa.simulation_time_sec    = sim_sec;
    sa.real_time_elapsed_ms   = real_elapsed_ms;
    sa.total_orders_placed    = total_placed;

    // BUG-4 FIX: compute cancelled count from actual order status instead of
    // always receiving 0 from the call site.
    for (const auto& o : om.allOrders())
        if (o.status == OrderStatus::CANCELLED) sa.total_cancelled++;

    // ── Build batch-frequency map once (used for both system & per-rider splits) ──
    // BUG-6 FIX: needed here so per-rider solo/batched counts can be derived.
    std::unordered_map<int, int> batch_freq;
    for (const auto& o : om.allOrders()) {
        if (o.status == OrderStatus::DELIVERED && o.batch_id >= 0)
            batch_freq[o.batch_id]++;
    }

    // ── Per-rider pass ───────────────────────────────────────
    for (const auto& rider : riders_) {
        RiderStats rs;
        rs.rider_id   = rider.id;
        rs.rider_name = rider.name;
        rs.orders_delivered   = rider.orders_delivered;
        rs.batches_completed  = rider.batches_completed;

        rs.actual_distance_km = rider.delivery_distance_m / 1000.0;
        rs.solo_distance_km   = rider.solo_distance_m / 1000.0;
        rs.distance_saved_km  = rider.route_distance_saved_m / 1000.0;

        rs.earnings_inr       = rider.earnings_inr;
        rs.solo_earnings_inr  = rider.solo_earnings_inr;
        rs.earnings_increase  = rider.earnings_inr - rider.solo_earnings_inr;
        rs.earnings_increase_pct = (rider.solo_earnings_inr > 0)
            ? (rs.earnings_increase / rider.solo_earnings_inr) * 100.0
            : 0.0;

        rs.fuel_cost_inr      = rider.fuel_cost_inr;
        rs.solo_fuel_cost_inr = rider.solo_fuel_cost_inr;
        rs.fuel_saved_inr     = rs.solo_fuel_cost_inr - rs.fuel_cost_inr;
        if (rs.fuel_saved_inr < 0) rs.fuel_saved_inr = 0;

        double hours = sim_sec / 3600.0;
        rs.orders_per_hour = (hours > 0) ? rider.orders_delivered / hours : 0.0;

        // BUG-6 FIX: compute per-rider solo vs. batched order split
        // BUG-5 FIX: accumulate delivery times for avg_delivery_time_min
        double delivery_time_sum_min = 0.0;
        int    delivery_time_count   = 0;
        for (const auto& o : om.allOrders()) {
            if (o.status != OrderStatus::DELIVERED) continue;
            if (o.rider_id != rider.id)             continue;
            // solo vs batched classification
            if (o.batch_id < 0 || batch_freq.count(o.batch_id) == 0 ||
                batch_freq.at(o.batch_id) == 1)
                rs.solo_orders++;
            else
                rs.batched_orders++;
            // delivery time
            if (o.delivered_at_sim_sec > 0 && o.placed_at_sim_sec >= 0) {
                delivery_time_sum_min +=
                    (o.delivered_at_sim_sec - o.placed_at_sim_sec) / 60.0;
                delivery_time_count++;
            }
        }
        // BUG-5 FIX: set per-rider average delivery time
        rs.avg_delivery_time_min = (delivery_time_count > 0)
            ? delivery_time_sum_min / delivery_time_count : 0.0;

        sa.rider_stats.push_back(rs);

        sa.total_orders_delivered    += rider.orders_delivered;
        sa.total_distance_km         += rs.actual_distance_km;
        sa.total_solo_distance_km    += rs.solo_distance_km;
        sa.total_distance_saved_km   += rs.distance_saved_km;
        sa.total_earnings_inr        += rs.earnings_inr;
        sa.total_solo_earnings_inr   += rs.solo_earnings_inr;
        sa.total_fuel_cost_inr       += rs.fuel_cost_inr;
        sa.total_fuel_saved_inr      += rs.fuel_saved_inr;
        sa.total_batches             += rider.batches_completed;
    }

    // ── System-wide batched / solo counts + avg delivery time ───────────────
    {
        int batched_count = 0, solo_count = 0;
        double delivery_time_sum_min = 0.0;
        int    delivery_time_count   = 0;
        for (const auto& o : om.allOrders()) {
            if (o.status != OrderStatus::DELIVERED) continue;
            if (o.batch_id < 0 || batch_freq.count(o.batch_id) == 0 ||
                batch_freq.at(o.batch_id) == 1)
                solo_count++;
            else
                batched_count++;
            // BUG-5 FIX: accumulate for system-wide avg delivery time
            if (o.delivered_at_sim_sec > 0 && o.placed_at_sim_sec >= 0) {
                delivery_time_sum_min +=
                    (o.delivered_at_sim_sec - o.placed_at_sim_sec) / 60.0;
                delivery_time_count++;
            }
        }
        sa.total_batched_orders = batched_count;
        sa.total_solo_orders    = solo_count;
        // BUG-5 FIX: set system-wide average delivery time
        sa.avg_delivery_time_min = (delivery_time_count > 0)
            ? delivery_time_sum_min / delivery_time_count : 0.0;

        if (sa.total_orders_delivered > 0)
            sa.batch_rate_pct = 100.0 * batched_count / sa.total_orders_delivered;
    }

    sa.total_earnings_increase = sa.total_earnings_inr - sa.total_solo_earnings_inr;
    sa.earnings_increase_pct   = (sa.total_solo_earnings_inr > 0)
        ? (sa.total_earnings_increase / sa.total_solo_earnings_inr) * 100.0
        : 0.0;

    if (sa.total_solo_distance_km > 0)
        sa.avg_distance_saved_pct =
            (sa.total_distance_saved_km / sa.total_solo_distance_km) * 100.0;

    double hours = sim_sec / 3600.0;
    sa.system_orders_per_hour = (hours > 0)
        ? sa.total_orders_delivered / hours : 0.0;

    return sa;
}
