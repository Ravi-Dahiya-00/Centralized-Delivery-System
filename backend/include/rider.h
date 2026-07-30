#pragma once
#include "order.h"
#include "route_optimizer.h"
#include <string>
#include <vector>
#include <deque>

// ─── Rider Status ─────────────────────────────────────────────
enum class RiderStatus {
    IDLE,           // waiting for orders
    MOVING_TO_PICKUP,
    PICKING_UP,     // at restaurant, collecting order
    MOVING_TO_DELIVERY,
    DELIVERING      // at customer door
};
std::string riderStatusToString(RiderStatus s);

// ─── Active Batch assigned to a rider ─────────────────────────
struct ActiveBatch {
    int                batch_id;
    std::vector<int>   order_ids;
    std::vector<int>   full_path;       // node-by-node route
    int                path_index = 0;  // current position in path
    int                total_distance_m;
    int                solo_distance_m; // sum of solo distances for all orders
    std::vector<Stop>  stops;           // ordered stops
    int                stops_done = 0;
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
    double total_distance_m     = 0.0;   // actual distance traveled
    double solo_distance_m      = 0.0;   // what distance would be if all solo
    double earnings_inr         = 0.0;   // actual earnings
    double solo_earnings_inr    = 0.0;   // what earnings would be without batching
    double fuel_cost_inr        = 0.0;   // actual fuel cost
    double solo_fuel_cost_inr   = 0.0;   // fuel cost without batching

    // ── Derived Metrics ────────────────────────────────────
    double distanceSaved()  const { return solo_distance_m  - total_distance_m; }
    double earningsBoosted() const { return earnings_inr    - solo_earnings_inr; }
    double fuelSaved()      const { return solo_fuel_cost_inr - fuel_cost_inr; }
};
