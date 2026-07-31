#pragma once
#include "order.h"
#include "route_optimizer.h"
#include <string>
#include <vector>
#include <deque>

// ─── Rider Status ─────────────────────────────────────────────
enum class RiderStatus {
    IDLE,                // waiting for orders
    MOVING_TO_PICKUP,    // riding to restaurant
    MOVING_TO_DELIVERY,  // riding to customer
    // NOTE: PICKING_UP and DELIVERING were removed (Bug 14 fix) —
    // riders never dwelt in those states; the transition was instant.
};
std::string riderStatusToString(RiderStatus s);

// ─── Active Batch assigned to a rider ─────────────────────────
struct ActiveBatch {
    int                batch_id;
    std::vector<int>   order_ids;
    std::vector<int>   full_path;       // node-by-node route
    int                path_index = 0;  // current position in path
    int                total_distance_m;
    int                solo_distance_m; // fair solo trips from rider start (same start as batch)
    std::vector<Stop>  stops;           // ordered stops
    int                stops_done = 0;
    bool               stats_applied = false; // batch-level analytics applied once
};

// ─── Rider ────────────────────────────────────────────────────
struct Rider {
    int         id;
    std::string name;
    int         current_node;    // current location in city graph
    double      x, y;            // interpolated visual position (for smooth animation)
    RiderStatus status = RiderStatus::IDLE;

    ActiveBatch* batch = nullptr;  // currently assigned batch (nullptr = none)

    // ── Zone assignment ──────────────────────────────────────
    int  zone_id    = 0;    // 0=West, 1=Central, 2=East
    int  zone_node  = -1;   // nearest intersection in home zone
    int  idle_ticks = 0;    // how many ticks spent idle without a new order
    // ── Lifetime Statistics ──────────────────────────────────
    int    orders_delivered     = 0;
    int    batches_completed    = 0;
    double total_distance_m     = 0.0;   // all travel (deliveries + zone reposition)
    double delivery_distance_m  = 0.0;   // distance on delivery batches only
    double solo_distance_m      = 0.0;   // fair solo baseline for delivered batches
    double earnings_inr         = 0.0;   // actual earnings (all delivered orders)
    double solo_earnings_inr    = 0.0;   // capacity-adjusted solo baseline
    double fuel_cost_inr        = 0.0;   // delivery fuel only
    double solo_fuel_cost_inr   = 0.0;   // solo-baseline fuel
    double route_distance_saved_m = 0.0; // fair_solo − batched_route (deliveries)

    // ── Derived Metrics ────────────────────────────────────
    double distanceSaved()  const { return route_distance_saved_m; }
    double earningsBoosted() const { return earnings_inr - solo_earnings_inr; }
    double fuelSaved()      const { return solo_fuel_cost_inr - fuel_cost_inr; }
};
