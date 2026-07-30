#pragma once
#include "graph.h"
#include <vector>
#include <string>

/*
 * Multi-Stop Route Optimizer
 * ──────────────────────────
 * Problem: Given a set of pickup and delivery stops (max ~6),
 *          find the optimal visiting order with the constraint:
 *          PICKUP_i must occur BEFORE DELIVERY_i.
 *
 * Approach: Brute-force permutation over all valid orderings.
 *           With max 6 stops → 6! = 720 permutations → trivially fast.
 *
 * This is a constrained mini-TSP problem.
 */

struct Stop {
    int  node_id;
    int  order_id;   // which order this stop belongs to
    bool is_pickup;  // true = pickup, false = delivery
    bool done = false; // BUG-4 fix: prevent re-processing at same node
};

struct OptimizedRoute {
    std::vector<int>  stop_sequence;   // ordered node IDs to visit
    std::vector<int>  full_path;       // full expanded path (all intermediate nodes)
    int               total_distance;  // total meters
    int               solo_distance;   // what distance would be if orders were solo
    double            savings_meters;  // solo_distance - total_distance
    std::string       description;     // human-readable route description
};

class RouteOptimizer {
public:
    explicit RouteOptimizer(const Graph& graph) : g(graph) {}

    /*
     * Given current rider position + list of stops (pickups and deliveries),
     * find the optimal valid ordering.
     * Constraint: For each order, its pickup stop must appear before its delivery stop.
     */
    OptimizedRoute optimize(int rider_node, const std::vector<Stop>& stops);

private:
    const Graph& g;

    // Check if a permutation of stops satisfies pickup-before-delivery constraint
    bool isValidOrder(const std::vector<int>& perm, const std::vector<Stop>& stops);

    // Compute total travel distance for a stop sequence starting from rider_node
    int computeRouteDistance(int start, const std::vector<int>& order_indices,
                             const std::vector<Stop>& stops);

    // Expand a stop sequence into full node-by-node path
    std::vector<int> expandPath(int start, const std::vector<int>& order_indices,
                                const std::vector<Stop>& stops);
};
