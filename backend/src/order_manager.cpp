#include "../include/order_manager.h"
#include <algorithm>
#include <iostream>

OrderManager::OrderManager(const Graph& g,
                           int max_orders_per_batch,
                           int max_pickup_dist_m,
                           int max_delivery_time_sec,
                           double extra_dist_threshold_pct,
                           int max_wait_before_solo_sec)
    : g(g),
      max_orders_per_batch(max_orders_per_batch),
      max_pickup_dist_m(max_pickup_dist_m),
      max_delivery_time_sec(max_delivery_time_sec),
      extra_dist_threshold_pct(extra_dist_threshold_pct),
      max_wait_sec(max_wait_before_solo_sec) {}

void OrderManager::enqueue(const Order& o) {
    orders.push_back(o);
    pending_ids.push_back(o.id);
    std::cout << "[OrderManager] Order #" << o.id
              << " enqueued (" << platformToString(o.platform)
              << "): node " << o.restaurant_node
              << " → node " << o.customer_node << "\n";
}

void OrderManager::markAssigned(int order_id) {
    pending_ids.erase(
        std::remove(pending_ids.begin(), pending_ids.end(), order_id),
        pending_ids.end()
    );
    for (auto& o : orders) {
        if (o.id == order_id) {
            o.status = OrderStatus::ASSIGNED;
            break;
        }
    }
}

Order* OrderManager::findOrder(int id) {
    for (auto& o : orders)
        if (o.id == id) return &o;
    return nullptr;
}

const Order* OrderManager::findOrder(int id) const {
    for (const auto& o : orders)
        if (o.id == id) return &o;
    return nullptr;
}



// ─── Compatibility Check ──────────────────────────────────────
bool OrderManager::areCompatible(const Order& a, const Order& b,
                                  long long /*sim_sec*/) const {
    // Rule 1: Pickup locations must be within max_pickup_dist_m of each other
    int pickup_dist = g.distance(a.restaurant_node, b.restaurant_node);
    if (pickup_dist > max_pickup_dist_m) return false;

    // Rule 2: Check if estimated batch delivery time is acceptable
    int est_dist = estimateBatchDistance(a, b);
    if (est_dist == Graph::INF) return false;
    int avg_speed_m_per_sec = 7;  // ~25 km/h
    int est_time = est_dist / avg_speed_m_per_sec;
    if (est_time > max_delivery_time_sec) return false;

    // Rule 3: Extra distance not too high vs. direct solo routes
    int solo_a = g.distance(a.restaurant_node, a.customer_node);
    int solo_b = g.distance(b.restaurant_node, b.customer_node);
    int solo_total = solo_a + solo_b;
    if (solo_total > 0) {
        double extra_pct = 100.0 * (est_dist - solo_total) / (double)solo_total;
        if (extra_pct > extra_dist_threshold_pct) return false;
    }
    return true;
}

int OrderManager::estimateBatchDistance(const Order& a, const Order& b) const {
    // Estimate: pickup A → pickup B → deliver A → deliver B
    int d1 = g.distance(a.restaurant_node, b.restaurant_node);
    int d2 = g.distance(b.restaurant_node, a.customer_node);
    int d3 = g.distance(a.customer_node,   b.customer_node);
    if (d1 == Graph::INF || d2 == Graph::INF || d3 == Graph::INF)
        return Graph::INF;
    return d1 + d2 + d3;
}

int OrderManager::estimateTripleDistance(const Order& a, const Order& b, const Order& c) const {
    // Estimate: pickup A → B → C → deliver A → B → C
    int d1 = g.distance(a.restaurant_node, b.restaurant_node);
    int d2 = g.distance(b.restaurant_node, c.restaurant_node);
    int d3 = g.distance(c.restaurant_node, a.customer_node);
    int d4 = g.distance(a.customer_node,   b.customer_node);
    int d5 = g.distance(b.customer_node,   c.customer_node);
    if (d1 == Graph::INF || d2 == Graph::INF || d3 == Graph::INF ||
        d4 == Graph::INF || d5 == Graph::INF)
        return Graph::INF;
    return d1 + d2 + d3 + d4 + d5;
}

// ─── Process Pending Orders → Form Batches ────────────────────
std::vector<BatchProposal> OrderManager::processPending(long long current_sim_sec) {
    std::vector<BatchProposal> proposals;

    // Build list of Order* for pending IDs
    std::vector<Order*> pending;
    for (int pid : pending_ids) {
        Order* op = findOrder(pid);
        if (op && op->status == OrderStatus::PENDING)
            pending.push_back(op);
    }

    std::vector<bool> matched(pending.size(), false);

    // ── Pass 1: Try to form TRIPLES (if allowed) ───────────────
    if (max_orders_per_batch >= 3) {
        for (int i = 0; i < (int)pending.size(); ++i) {
            if (matched[i]) continue;
            for (int j = i + 1; j < (int)pending.size(); ++j) {
                if (matched[j]) continue;
                if (!areCompatible(*pending[i], *pending[j], current_sim_sec)) continue;
                for (int k = j + 1; k < (int)pending.size(); ++k) {
                    if (matched[k]) continue;
                    if (areCompatible(*pending[i], *pending[k], current_sim_sec) &&
                        areCompatible(*pending[j], *pending[k], current_sim_sec)) {

                        int est = estimateTripleDistance(*pending[i], *pending[j], *pending[k]);
                        if (est == Graph::INF) continue;

                        BatchProposal bp;
                        bp.order_ids = {pending[i]->id, pending[j]->id, pending[k]->id};
                        int si = g.distance(pending[i]->restaurant_node, pending[i]->customer_node);
                        int sj = g.distance(pending[j]->restaurant_node, pending[j]->customer_node);
                        int sk = g.distance(pending[k]->restaurant_node, pending[k]->customer_node);
                        bp.solo_total_distance_m      = si + sj + sk;
                        bp.total_estimated_distance_m = est;
                        bp.efficiency_score = (bp.solo_total_distance_m > 0)
                            ? (double)(bp.solo_total_distance_m - est) / bp.solo_total_distance_m
                            : 0.0;
                        proposals.push_back(bp);
                        matched[i] = matched[j] = matched[k] = true;

                        std::cout << "[Batch-3] Orders #" << pending[i]->id
                                  << " + #" << pending[j]->id << " + #" << pending[k]->id
                                  << " | Solo: " << bp.solo_total_distance_m << "m"
                                  << " | Triple: " << est << "m"
                                  << " | Saving: " << (bp.solo_total_distance_m - est) << "m\n";
                        break;
                    }
                }
                if (matched[i]) break;
            }
        }
    }

    // ── Pass 2: Try to form PAIRS from remaining ───────────────
    for (int i = 0; i < (int)pending.size(); ++i) {
        if (matched[i]) continue;
        Order& oa = *pending[i];
        bool found_pair = false;

        for (int j = i + 1; j < (int)pending.size(); ++j) {
            if (matched[j]) continue;
            Order& ob = *pending[j];
            if (areCompatible(oa, ob, current_sim_sec)) {
                BatchProposal bp;
                bp.order_ids = {oa.id, ob.id};
                int solo_a = g.distance(oa.restaurant_node, oa.customer_node);
                int solo_b = g.distance(ob.restaurant_node, ob.customer_node);
                bp.solo_total_distance_m      = solo_a + solo_b;
                bp.total_estimated_distance_m = estimateBatchDistance(oa, ob);
                bp.efficiency_score = (bp.solo_total_distance_m > 0)
                    ? (double)(bp.solo_total_distance_m - bp.total_estimated_distance_m)
                      / bp.solo_total_distance_m
                    : 0.0;
                proposals.push_back(bp);
                matched[i] = matched[j] = true;
                found_pair = true;

                std::cout << "[Batch-2] Orders #" << oa.id << " + #" << ob.id
                          << " | Solo: " << bp.solo_total_distance_m << "m"
                          << " | Batch: " << bp.total_estimated_distance_m << "m"
                          << " | Saving: "
                          << (bp.solo_total_distance_m - bp.total_estimated_distance_m) << "m\n";
                break;
            }
        }

        // If no pair found, check if order waited too long → send solo
        if (!found_pair) {
            long long waited = current_sim_sec - oa.placed_at_sim_sec;
            if (waited >= max_wait_sec) {
                BatchProposal bp;
                bp.order_ids = {oa.id};
                int solo = g.distance(oa.restaurant_node, oa.customer_node);
                bp.solo_total_distance_m      = solo;
                bp.total_estimated_distance_m = solo;
                bp.efficiency_score           = 0.0;
                proposals.push_back(bp);
                matched[i] = true;
                std::cout << "[Solo] Order #" << oa.id
                          << " dispatched solo after " << waited << "s wait.\n";
            }
        }
    }
    return proposals;
}
