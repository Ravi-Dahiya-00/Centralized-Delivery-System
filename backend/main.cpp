/*
 * Centralized Delivery System — Main Entry Point
 * ================================================
 * DSA College Project
 *
 * Architecture:
 *  - Thread 1 (main): Simulation loop — ticks every 800ms
 *  - Thread 2      : HTTP API server (blocking httplib::Server::listen)
 *
 * The simulation loop runs independently, advancing virtual time and
 * updating rider positions. The API server serves the current state
 * on every GET /api/state request.
 *
 * Mutex guards shared data (orders, riders) between threads.
 */

#include "include/graph.h"
#include "include/order_manager.h"
#include "include/rider_manager.h"
#include "include/simulator.h"
#include "include/api_server.h"
#include "vendor/json.hpp"
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <iostream>
#include <csignal>

using json = nlohmann::json;

// ─── Global mutex for shared state ───────────────────────────
std::mutex g_state_mutex;
std::atomic<bool> g_running{true};

void signalHandler(int) {
    std::cout << "\n[Main] Shutting down...\n";
    g_running = false;
}

// ─── Load config from city_graph.json ────────────────────────
SimConfig loadSimConfig(const std::string& path) {
    std::ifstream f(path);
    json j;
    f >> j;
    SimConfig cfg;
    if (j.contains("config")) {
        auto& c = j["config"];
        cfg.order_interval_sec    = c.value("order_interval_seconds", 12);
        cfg.max_delivery_time_sec = c.value("max_delivery_time_seconds", 2700);
        cfg.tick_interval_ms      = c.value("sim_tick_ms", 800);
        cfg.turbo_duration_sec    = 28800;
    }
    return cfg;
}

// ─── Load riders from city_graph.json ────────────────────────
std::vector<Rider> loadRiders(const std::string& path, const Graph& g) {
    std::ifstream f(path);
    json j;
    f >> j;
    std::vector<Rider> riders;
    for (const auto& jr : j["riders"]) {
        Rider r;
        r.id           = jr["id"].get<int>();
        r.name         = jr["name"].get<std::string>();
        r.current_node = jr["start_node"].get<int>();
        r.x            = g.nodes[r.current_node].x;
        r.y            = g.nodes[r.current_node].y;
        r.status       = RiderStatus::IDLE;
        riders.push_back(r);
    }
    return riders;
}

int main() {
    std::signal(SIGINT, signalHandler);

    const std::string GRAPH_PATH = "data/city_graph.json";
    const int         API_PORT   = 8081;

    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║   Centralized Delivery System — DSA Project  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n\n";

#ifdef _WIN32
    // Initialize Winsock for httplib on Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    // ── Load city graph ──────────────────────────────────────
    std::cout << "[Main] Loading city graph from: " << GRAPH_PATH << "\n";
    Graph g = Graph::loadFromFile(GRAPH_PATH);
    std::cout << "[Main] Graph loaded: " << g.nodeCount() << " nodes\n\n";

    // ── Load config ──────────────────────────────────────────
    SimConfig cfg = loadSimConfig(GRAPH_PATH);
    std::cout << "[Main] Config: order_interval=" << cfg.order_interval_sec
              << "s, tick=" << cfg.tick_interval_ms << "ms\n";

    // ── Read pay config ──────────────────────────────────────
    double base_pay = 35.0, dist_bonus = 2.0, fuel_rate = 1.8;
    int max_batch = 3, max_pickup_m = 500;
    double extra_pct = 40.0;
    int solo_wait_s = 15;   // default: send solo after 15 sim-seconds without a pair
    {
        std::ifstream f(GRAPH_PATH);
        json j; f >> j;
        if (j.contains("config")) {
            base_pay     = j["config"].value("order_base_pay_inr",          35.0);
            dist_bonus   = j["config"].value("distance_bonus_per_km_inr",    2.0);
            fuel_rate    = j["config"].value("fuel_cost_per_km_inr",         1.8);
            max_batch    = j["config"].value("max_orders_per_batch",           3);
            max_pickup_m = j["config"].value("max_batch_pickup_distance",    500);
            extra_pct    = j["config"].value("extra_distance_threshold_pct", 40.0);
            solo_wait_s  = j["config"].value("solo_wait_before_solo_sec",     15);   // BUG-3 FIX: was hardcoded
        }
    }

    // ── Create managers ──────────────────────────────────────
    OrderManager om(g,
        max_batch,
        max_pickup_m,
        cfg.max_delivery_time_sec,
        extra_pct,
        solo_wait_s
    );

    RiderManager rm(g, om, base_pay, dist_bonus, fuel_rate);

    // Load riders
    for (const auto& r : loadRiders(GRAPH_PATH, g))
        rm.addRider(r);
    std::cout << "[Main] " << rm.riders().size() << " riders loaded.\n";

    // Assign home zones — distributes riders across West / Central / East
    rm.assignZones();
    std::cout << "[Main] Zones assigned.\n";

    // ── Create simulator ─────────────────────────────────────
    Simulator sim(g, om, cfg);

    // ── Start API server in a background thread ──────────────
    ApiServer api(g, om, rm, sim, API_PORT, g_state_mutex);  // BUG-2 fix: pass mutex
    std::thread api_thread([&api]() {
        api.run();
    });
    api_thread.detach();

    std::cout << "\n[Main] API Server started on port " << API_PORT << "\n";
    std::cout << "[Main] Open http://localhost:5173 in your browser\n";
    std::cout << "[Main] Simulation loop running (Ctrl+C to stop)\n\n";

    // ── Main simulation loop ──────────────────────────────────
    auto last_tick = std::chrono::steady_clock::now();

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_tick).count();

        if (elapsed_ms >= cfg.tick_interval_ms) {
            last_tick = now;

            std::lock_guard<std::mutex> lock(g_state_mutex);

            // 1. Advance simulation clock
            long long sim_sec;
            sim.tick(sim_sec);

            // 2. Process pending orders → form batches
            if (sim.state() == SimState::RUNNING) {
                auto proposals = om.processPending(sim_sec);
                for (const auto& proposal : proposals) {
                    int batch_id = -1;
                    rm.assignBatch(proposal, sim_sec, batch_id);
                }

                // 3. Advance all riders one step
                rm.tick(sim_sec);
            }
        }

        // Sleep a little to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "[Main] Simulation stopped.\n";
    return 0;
}
