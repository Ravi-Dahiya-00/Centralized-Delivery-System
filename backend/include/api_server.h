#pragma once
#include "graph.h"
#include "order_manager.h"
#include "rider_manager.h"
#include "simulator.h"
#include "analytics.h"
#include "../vendor/json.hpp"
#include <mutex>

using json = nlohmann::json;

/*
 * API Server
 * ──────────
 * Uses cpp-httplib (single-header HTTP server).
 * Serves REST endpoints on port 8081.
 * Frontend polls /api/state every 500ms for live updates.
 *
 * Endpoints:
 *   GET  /api/graph           → full city graph (static)
 *   GET  /api/state           → live state (orders, riders, analytics)
 *   POST /api/sim/start       → start simulation
 *   POST /api/sim/stop        → stop simulation
 *   POST /api/sim/pause       → pause
 *   POST /api/sim/resume      → resume
 *   POST /api/sim/reset       → reset all state
 *   POST /api/sim/speed       → set speed { "multiplier": 5.0 }
 *   POST /api/sim/turbo       → fast-forward { "hours": 8 }
 *   GET  /api/analytics       → detailed analytics snapshot
 *
 * BUG-2 FIX: All GET endpoints now lock g_state_mutex before reading shared data.
 */

class ApiServer {
public:
    // BUG-2 FIX: takes shared mutex reference to guard concurrent reads
    ApiServer(Graph& g, OrderManager& om, RiderManager& rm,
              Simulator& sim, int port, std::mutex& state_mutex);

    // Starts the HTTP server (blocking call)
    void run();

    // Build JSON of current live state (orders + riders + analytics)
    // MUST be called with state_mutex held
    json buildState();

private:
    Graph&        g;
    OrderManager& om;
    RiderManager& rm;
    Simulator&    sim;
    int           port_;
    std::mutex&   mutex_;  // BUG-2 FIX: reference to shared state mutex

    long long start_real_ms_; // wall clock when server started

    // JSON serializers
    json graphToJson()     const;
    json orderToJson(const Order& o)   const;
    json riderToJson(const Rider& r)   const;
    json analyticsToJson(const SystemAnalytics& sa) const;

    long long nowMs() const;
};
