#pragma once
#include <string>
#include <chrono>

// ─── Platform ─────────────────────────────────────────────────
enum class Platform { ZOMATO, SWIGGY };
std::string platformToString(Platform p);

// ─── Order Status ─────────────────────────────────────────────
enum class OrderStatus {
    PENDING,     // waiting for assignment
    ASSIGNED,    // assigned to a batch/rider, not yet picked up
    PICKED_UP,   // rider has reached restaurant
    DELIVERED,   // delivered to customer
    CANCELLED    // timed out (no rider found)
};
std::string orderStatusToString(OrderStatus s);

// ─── Order ────────────────────────────────────────────────────
struct Order {
    int        id;
    Platform   platform;
    int        restaurant_node;    // pickup node ID
    int        customer_node;      // delivery node ID
    long long  placed_at_sim_sec;  // simulation time when order was placed
    long long  assigned_at_sim_sec = -1;
    long long  picked_up_at_sim_sec = -1;
    long long  delivered_at_sim_sec = -1;
    long long  deadline_sim_sec;   // must be delivered before this sim time

    OrderStatus status = OrderStatus::PENDING;
    int         batch_id = -1;     // -1 = not batched
    int         rider_id = -1;

    // Distances (filled when assigned)
    int         solo_distance_m = 0;    // shortest distance if delivered alone
    int         actual_distance_m = 0;  // actual distance in batch route

    // Helper
    long long waitTime()      const { return assigned_at_sim_sec  - placed_at_sim_sec; }
    long long deliveryTime()  const { return delivered_at_sim_sec - placed_at_sim_sec; }
    int       savedDistance() const { return solo_distance_m - actual_distance_m; }
};
