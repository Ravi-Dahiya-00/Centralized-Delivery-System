#include "../include/route_optimizer.h"
#include <algorithm>
#include <numeric>
#include <climits>
#include <sstream>
#include <unordered_set>


/*
 * Route Optimizer — Brute-Force Constrained TSP
 * ───────────────────────────────────────────────
 * With max 4 stops (2 pickups + 2 deliveries), we have 4! = 24 permutations.
 * After applying the pickup-before-delivery constraint, only a few are valid.
 * We simply evaluate all valid permutations and pick the one with minimum distance.
 *
 * Constraint enforcement:
 *   For each permutation, scan left to right.
 *   Track which orders have been "picked up" so far.
 *   A delivery stop for order X is only valid if X's pickup has already been seen.
 */

// ─── Check if a permutation satisfies pickup-before-delivery ──
bool RouteOptimizer::isValidOrder(const std::vector<int>& perm,
                                  const std::vector<Stop>& stops) {
    // BUG-3 fix: use unordered_set — safe for any order_id value
    std::unordered_set<int> picked_up;
    for (int idx : perm) {
        const Stop& s = stops[idx];
        if (s.is_pickup) {
            picked_up.insert(s.order_id);
        } else {
            // Delivery: corresponding pickup must have happened first
            if (!picked_up.count(s.order_id)) return false;
        }
    }
    return true;
}


// ─── Compute total distance for a given stop ordering ─────────
int RouteOptimizer::computeRouteDistance(int start,
                                          const std::vector<int>& order_indices,
                                          const std::vector<Stop>& stops) {
    int total = 0;
    int cur = start;
    for (int idx : order_indices) {
        int nxt = stops[idx].node_id;
        int d = g.distance(cur, nxt);
        if (d == Graph::INF) return Graph::INF;
        total += d;
        cur = nxt;
    }
    return total;
}

// ─── Expand stop sequence into full node-by-node path ─────────
std::vector<int> RouteOptimizer::expandPath(int start,
                                             const std::vector<int>& order_indices,
                                             const std::vector<Stop>& stops) {
    std::vector<int> full_path = {start};
    int cur = start;
    for (int idx : order_indices) {
        int nxt = stops[idx].node_id;
        if (cur == nxt) continue;
        std::vector<int> seg = g.getPath(cur, nxt);
        if (seg.size() > 1)
            full_path.insert(full_path.end(), seg.begin() + 1, seg.end());
        cur = nxt;
    }
    return full_path;
}

// ─── Main optimize function ───────────────────────────────────
OptimizedRoute RouteOptimizer::optimize(int rider_node,
                                        const std::vector<Stop>& stops) {
    int n = (int)stops.size();
    OptimizedRoute result;

    if (n == 0) {
        result.stop_sequence = {};
        result.full_path     = {rider_node};
        result.total_distance = 0;
        return result;
    }

    // Create index array [0, 1, ..., n-1] to permute
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    int best_dist   = INT_MAX;
    std::vector<int> best_perm;

    // Try all permutations — for n ≤ 6, this is at most 720 iterations
    do {
        if (!isValidOrder(indices, stops)) continue;
        int d = computeRouteDistance(rider_node, indices, stops);
        if (d < best_dist) {
            best_dist = d;
            best_perm = indices;
        }
    } while (std::next_permutation(indices.begin(), indices.end()));

    result.total_distance = best_dist;

    if (!best_perm.empty()) {
        // Build stop_sequence (node IDs in visit order)
        for (int idx : best_perm)
            result.stop_sequence.push_back(stops[idx].node_id);

        // Build full path with all intermediate nodes
        result.full_path = expandPath(rider_node, best_perm, stops);\

        // BUG-13 FIX: compute solo_distance and savings_meters.
        // solo_distance = sum of (pickup → delivery) for each order independently.
        // We pair each delivery stop with its matching pickup stop.
        int solo_dist = 0;
        for (const auto& s : stops) {
            if (s.is_pickup) {
                // Find the delivery stop for the same order_id
                for (const auto& d : stops) {
                    if (!d.is_pickup && d.order_id == s.order_id) {
                        int seg = g.distance(s.node_id, d.node_id);
                        if (seg != Graph::INF) solo_dist += seg;
                        break;
                    }
                }
            }
        }
        result.solo_distance  = solo_dist;
        result.savings_meters = (double)solo_dist - (double)best_dist;
        if (result.savings_meters < 0) result.savings_meters = 0.0;

        // Human-readable description
        std::ostringstream desc;
        desc << "Rider@" << rider_node;
        for (int idx : best_perm) {
            desc << " → " << (stops[idx].is_pickup ? "PICKUP" : "DELIVER")
                 << "@" << stops[idx].node_id
                 << "(order#" << stops[idx].order_id << ")";
        }
        result.description = desc.str();
    }

    return result;
}
