#include "../include/simulator.h"
#include "../include/rider_manager.h"
#include "../include/order.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <unordered_map>

// ─── Rush Hour Demand Multiplier ──────────────────────────────────
// Returns a value 0.0–1.0 representing "how often" to generate orders.
// 1.0 = normal rate, 2.0 = twice as often, 0.3 = third of normal rate.
static double getRushMultiplier(long long sim_sec) {
    // Map simulation seconds to hour-of-day (0–23)
    int hour = (int)((sim_sec % 86400) / 3600);

    // Night       00–05 : very quiet   0.2×
    // Breakfast   07–09 : moderate     1.5×
    // Lunch       12–14 : PEAK         3.0×
    // Afternoon   15–17 : moderate     1.2×
    // Dinner      19–22 : HEAVY PEAK   2.5×
    // Rest                             0.7×
    if (hour >= 0  && hour < 6)  return 0.2;   // night
    if (hour >= 6  && hour < 7)  return 0.6;   // early morning
    if (hour >= 7  && hour < 10) return 1.5;   // breakfast rush
    if (hour >= 10 && hour < 12) return 0.9;   // mid-morning
    if (hour >= 12 && hour < 15) return 3.0;   // LUNCH PEAK
    if (hour >= 15 && hour < 17) return 1.2;   // afternoon snack
    if (hour >= 17 && hour < 19) return 1.0;   // early evening
    if (hour >= 19 && hour < 23) return 2.5;   // DINNER PEAK
    return 0.4;                                 // late night
}

Simulator::Simulator(const Graph& g, OrderManager& om, SimConfig cfg)
    : g(g), om(om), cfg_(cfg), rng_(std::random_device{}()) {
    buildNodeLists();
}

void Simulator::buildNodeLists() {
    for (const auto& node : g.nodes) {
        switch (node.type) {
            case NodeType::ZOMATO: zomato_nodes_.push_back(node.id); break;
            case NodeType::SWIGGY: swiggy_nodes_.push_back(node.id); break;
            case NodeType::BOTH:
                zomato_nodes_.push_back(node.id);
                swiggy_nodes_.push_back(node.id);
                both_nodes_.push_back(node.id);
                break;
            case NodeType::HOUSE:  house_nodes_.push_back(node.id); break;
            default: break;
        }
    }
    std::cout << "[Sim] Node lists: Zomato=" << zomato_nodes_.size()
              << " Swiggy=" << swiggy_nodes_.size()
              << " Houses=" << house_nodes_.size() << "\n";
}

void Simulator::generateOrder() {
    if (house_nodes_.empty()) return;

    // Randomly pick platform
    std::uniform_int_distribution<int> coin(0, 1);
    Platform platform = (coin(rng_) == 0) ? Platform::ZOMATO : Platform::SWIGGY;

    const std::vector<int>& rest_list =
        (platform == Platform::ZOMATO) ? zomato_nodes_ : swiggy_nodes_;

    if (rest_list.empty()) return;

    // Pick random restaurant
    std::uniform_int_distribution<int> rest_dist(0, (int)rest_list.size() - 1);
    int restaurant_node = rest_list[rest_dist(rng_)];

    // Pick random house (different from restaurant)
    int customer_node;
    int attempts = 0;
    do {
        std::uniform_int_distribution<int> house_dist(0, (int)house_nodes_.size() - 1);
        customer_node = house_nodes_[house_dist(rng_)];
        attempts++;
    } while (customer_node == restaurant_node && attempts < 10);

    // BUG-14 FIX: if we still ended up with the same node, drop the order
    if (customer_node == restaurant_node) return;

    Order o;
    o.id               = om.nextOrderId();
    o.platform         = platform;
    o.restaurant_node  = restaurant_node;
    o.customer_node    = customer_node;
    o.placed_at_sim_sec = sim_time_sec_;
    o.deadline_sim_sec  = sim_time_sec_ + cfg_.max_delivery_time_sec;
    o.status           = OrderStatus::PENDING;

    om.enqueue(o);
    total_generated_++;
    last_order_sim_sec = sim_time_sec_;
}

void Simulator::start()  { state_ = SimState::RUNNING; }
void Simulator::pause()  { state_ = SimState::PAUSED;  }
void Simulator::resume() { state_ = SimState::RUNNING; }
void Simulator::stop()   {
    state_ = SimState::STOPPED;
}

void Simulator::reset() {
    state_          = SimState::STOPPED;
    sim_time_sec_   = 0;
    last_order_sim_sec = 0;
    total_generated_ = 0;
}

void Simulator::setSpeed(double multiplier) {
    cfg_.speed_multiplier = multiplier;
}

// Expose the current rush-hour multiplier for the API (single source of truth).
double Simulator::rushMultiplier() const {
    return getRushMultiplier(sim_time_sec_);
}

// ─── Normal tick ──────────────────────────────────────────────
bool Simulator::tick(long long& out_sim_sec) {
    if (state_ != SimState::RUNNING && state_ != SimState::TURBO) {
        out_sim_sec = sim_time_sec_;
        return false;
    }

    // Advance simulation time by (tick_ms * speed_multiplier / 1000) seconds
    long long sim_advance = (long long)(cfg_.tick_interval_ms * cfg_.speed_multiplier / 1000.0);
    if (sim_advance < 1) sim_advance = 1;
    sim_time_sec_ += sim_advance;

    out_sim_sec = sim_time_sec_;

    // Dynamic interval based on rush hour multiplier
    // Higher multiplier → shorter interval → more orders
    double rush = getRushMultiplier(sim_time_sec_);
    long long effective_interval = (long long)(cfg_.order_interval_sec / std::max(rush, 0.05));
    if (effective_interval < 1) effective_interval = 1;

    // Generate order if interval elapsed
    bool generated = false;
    if (sim_time_sec_ - last_order_sim_sec >= effective_interval) {
        generateOrder();
        generated = true;
    }
    return generated;
}

// ─── Turbo Mode: fast-forward N hours using the same logic as main loop ──
static void runDeliveryStep(OrderManager& om, RiderManager& rm, long long sim_sec) {
    auto proposals = om.processPending(sim_sec);
    for (const auto& proposal : proposals) {
        int batch_id = -1;
        rm.assignBatch(proposal, sim_sec, batch_id);
    }
    rm.tick(sim_sec);
}

static long long simAdvanceSec(const SimConfig& cfg) {
    long long sim_advance =
        (long long)(cfg.tick_interval_ms * cfg.speed_multiplier / 1000.0);
    return sim_advance < 1 ? 1 : sim_advance;
}

static bool hasDeliveryWork(const OrderManager& om, const RiderManager& rm) {
    return om.hasPendingOrders() || rm.hasActiveRiders();
}

int Simulator::turboSimulate(int hours, RiderManager& rm) {
    state_ = SimState::TURBO;
    long long end_time = sim_time_sec_ + (long long)hours * 3600;
    long long sim_advance = simAdvanceSec(cfg_);

    int orders_before = total_generated_;

    std::cout << "[Turbo] Fast-forwarding " << hours << " hours ("
              << end_time << " sim sec) using normal tick logic...\n";

    // Phase 1: same loop as main.cpp — tick → batch → assign → move (1 edge/tick)
    while (sim_time_sec_ < end_time) {
        long long sim_sec;
        tick(sim_sec);
        runDeliveryStep(om, rm, sim_sec);
    }

    // Phase 2: finish in-flight deliveries (no new orders)
    long long drain_limit = end_time + (long long)cfg_.max_delivery_time_sec * 2;
    while (hasDeliveryWork(om, rm) && sim_time_sec_ < drain_limit) {
        sim_time_sec_ += sim_advance;
        runDeliveryStep(om, rm, sim_time_sec_);
    }

    int generated = total_generated_ - orders_before;

    // BUG-7 FIX: capture per-rider delivered counts BEFORE turbo started (stored
    // as orders_before equivalents) so we can report a delta, not all-time totals.
    // We stored nothing before, so derive the delta from the order list instead —
    // count DELIVERED orders whose placed_at_sim_sec falls in the turbo window.
    long long turbo_start_sec = end_time - (long long)hours * 3600;
    int delivered = 0;
    int batched   = 0;
    std::unordered_map<int, int> batch_freq;
    for (const auto& o : om.allOrders()) {
        if (o.status == OrderStatus::DELIVERED && o.batch_id >= 0)
            batch_freq[o.batch_id]++;
    }
    for (const auto& o : om.allOrders()) {
        if (o.status != OrderStatus::DELIVERED) continue;
        if (o.placed_at_sim_sec < turbo_start_sec) continue;  // from a previous run
        delivered++;
        if (o.batch_id >= 0 && batch_freq[o.batch_id] > 1)
            batched++;
    }

    std::cout << "[Turbo] Done. Generated " << generated
              << ", delivered " << delivered
              << ", batched " << batched
              << " (" << (delivered > 0 ? 100 * batched / delivered : 0) << "%)"
              << ", sim time=" << sim_time_sec_ << "s\n";

    state_ = SimState::STOPPED;
    return generated;
}
