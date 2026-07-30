#pragma once
#include "graph.h"
#include "order_manager.h"
#include <random>
#include <string>

/*
 * Simulator
 * ─────────
 * Controls the simulation clock and randomly generates orders.
 *
 * SIMULATION TIME:
 *  - sim_time_sec: "virtual" city time in seconds
 *  - real_time_ms: actual wall clock time elapsed
 *  - speed_multiplier: 1.0 = real time, 10.0 = 10x fast, etc.
 *
 * TURBO MODE (8-hour fast forward):
 *  - Runs 8 * 3600 = 28,800 simulation seconds instantly
 *  - Processes all ticks at max speed (no sleep between ticks)
 *  - Useful for generating a full day's data quickly
 *
 * ORDER GENERATION:
 *  - Orders generated every `order_interval_sec` sim seconds
 *  - Platform (Zomato/Swiggy) chosen randomly
 *  - Restaurant chosen from compatible platform restaurants
 *  - Customer chosen from HOUSE nodes
 *  - Avoids same restaurant = customer (trivially)
 */

enum class SimState { STOPPED, RUNNING, TURBO, PAUSED };

struct SimConfig {
    double speed_multiplier      = 1.0;   // 1x = real time
    int    order_interval_sec    = 12;    // sim seconds between orders
    int    max_delivery_time_sec = 2700;  // 45 minutes
    int    tick_interval_ms      = 800;   // real ms per simulation tick
    long long turbo_duration_sec = 28800; // 8 hours in seconds
};

class Simulator {
public:
    Simulator(const Graph& g, OrderManager& om, SimConfig cfg);

    // Start normal simulation
    void start();

    // Pause / resume
    void pause();
    void resume();

    // Stop and reset
    void stop();

    // Instantly simulate N simulation-hours worth of orders
    // Returns how many orders were generated
    int turboSimulate(int hours = 8);

    // Set speed multiplier (1x, 2x, 5x, 10x)
    void setSpeed(double multiplier);

    // Called by the main simulation loop every real tick
    // Returns true if a new order was generated this tick
    bool tick(long long& out_sim_sec);

    // Getters
    SimState   state()          const { return state_; }
    long long  simTimeSec()     const { return sim_time_sec_; }
    SimConfig& config()               { return cfg_; }

    int totalOrdersGenerated()  const { return total_generated_; }

private:
    const Graph&   g;
    OrderManager&  om;
    SimConfig      cfg_;
    SimState       state_ = SimState::STOPPED;

    long long sim_time_sec_      = 0;   // virtual time
    long long last_order_sim_sec = 0;   // when last order was created (sim time)
    int       total_generated_   = 0;

    std::mt19937               rng_;
    std::vector<int>           zomato_nodes_;
    std::vector<int>           swiggy_nodes_;
    std::vector<int>           both_nodes_;
    std::vector<int>           house_nodes_;

    void buildNodeLists();

    // Generate and enqueue one random order
    void generateOrder();
};
