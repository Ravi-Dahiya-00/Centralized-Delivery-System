// Must define Windows version BEFORE including httplib (requires Windows 10+)
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00
#endif

#include "../include/api_server.h"
#include "../vendor/httplib.h"
#include <chrono>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

ApiServer::ApiServer(Graph& g, OrderManager& om, RiderManager& rm,
                     Simulator& sim, int port, std::mutex& state_mutex)
    : g(g), om(om), rm(rm), sim(sim), port_(port), mutex_(state_mutex) {
    start_real_ms_ = nowMs();
}

long long ApiServer::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ─── JSON Serializers ─────────────────────────────────────────
json ApiServer::graphToJson() const {
    json jg;
    jg["city_name"] = "SimCity";

    json nodes = json::array();
    for (const auto& n : g.nodes) {
        nodes.push_back({
            {"id",   n.id},
            {"name", n.name},
            {"type", nodeTypeToString(n.type)},
            {"x",    n.x},
            {"y",    n.y}
        });
    }
    jg["nodes"] = nodes;

    json edges = json::array();
    for (const auto& [from, nbrs] : g.adj) {
        for (const auto& e : nbrs) {
            if (from < e.to) {   // avoid duplicates
                edges.push_back({
                    {"from",   from},
                    {"to",     e.to},
                    {"weight", e.weight}
                });
            }
        }
    }
    jg["edges"] = edges;
    return jg;
}

json ApiServer::orderToJson(const Order& o) const {
    return {
        {"id",               o.id},
        {"platform",         platformToString(o.platform)},
        {"restaurant_node",  o.restaurant_node},
        {"restaurant_name",  g.nodes[o.restaurant_node].name},
        {"customer_node",    o.customer_node},
        {"customer_name",    g.nodes[o.customer_node].name},
        {"status",           orderStatusToString(o.status)},
        {"batch_id",         o.batch_id},
        {"rider_id",         o.rider_id},
        {"placed_at",        o.placed_at_sim_sec},
        {"assigned_at",      o.assigned_at_sim_sec},
        {"picked_up_at",     o.picked_up_at_sim_sec},
        {"delivered_at",     o.delivered_at_sim_sec},
        {"solo_distance_m",  o.solo_distance_m},
        {"actual_distance_m",o.actual_distance_m},
        {"distance_saved_m", o.savedDistance()}
    };
}

json ApiServer::riderToJson(const Rider& r) const {
    json jr = {
        {"id",              r.id},
        {"name",            r.name},
        {"current_node",    r.current_node},
        {"x",               r.x},
        {"y",               r.y},
        {"status",          riderStatusToString(r.status)},
        {"orders_delivered",r.orders_delivered},
        {"total_distance_km", r.total_distance_m / 1000.0},
        {"earnings_inr",    r.earnings_inr},
        {"solo_earnings_inr", r.solo_earnings_inr},
        {"earnings_increase",  r.earningsBoosted()},
        {"distance_saved_km", r.distanceSaved() / 1000.0},
        {"fuel_saved_inr",  r.fuelSaved()},
        {"zone_id",         r.zone_id},
        {"zone_node",       r.zone_node},
        {"idle_ticks",      r.idle_ticks},
    };
    if (r.batch) {
        jr["batch_id"] = r.batch->batch_id;
        jr["route"]    = r.batch->full_path;
        jr["path_idx"] = r.batch->path_index;
        jr["batch_orders"] = r.batch->order_ids;
    } else {
        jr["batch_id"] = nullptr;
        jr["route"]    = json::array();
    }
    return jr;
}

json ApiServer::analyticsToJson(const SystemAnalytics& sa) const {
    json ja = {
        {"total_orders_placed",     sa.total_orders_placed},
        {"total_orders_delivered",  sa.total_orders_delivered},
        {"total_batches",           sa.total_batches},
        {"total_batched_orders",    sa.total_batched_orders},
        {"total_solo_orders",       sa.total_solo_orders},
        {"total_cancelled",         sa.total_cancelled},
        {"total_distance_km",       sa.total_distance_km},
        {"total_solo_distance_km",  sa.total_solo_distance_km},
        {"total_distance_saved_km", sa.total_distance_saved_km},
        {"avg_distance_saved_pct",  sa.avg_distance_saved_pct},
        {"total_earnings_inr",      sa.total_earnings_inr},
        {"total_solo_earnings_inr", sa.total_solo_earnings_inr},
        {"total_earnings_increase", sa.total_earnings_increase},
        {"earnings_increase_pct",   sa.earnings_increase_pct},
        {"total_fuel_cost_inr",     sa.total_fuel_cost_inr},
        {"total_fuel_saved_inr",    sa.total_fuel_saved_inr},
        {"batch_rate_pct",          sa.batch_rate_pct},
        {"avg_delivery_time_min",   sa.avg_delivery_time_min},
        {"system_orders_per_hour",  sa.system_orders_per_hour},
        {"simulation_time_sec",     sa.simulation_time_sec},
        {"real_time_elapsed_ms",    sa.real_time_elapsed_ms}
    };

    json rider_stats = json::array();
    for (const auto& rs : sa.rider_stats) {
        rider_stats.push_back({
            {"rider_id",              rs.rider_id},
            {"rider_name",            rs.rider_name},
            {"orders_delivered",      rs.orders_delivered},
            {"batches_completed",     rs.batches_completed},
            {"actual_distance_km",    rs.actual_distance_km},
            {"solo_distance_km",      rs.solo_distance_km},
            {"distance_saved_km",     rs.distance_saved_km},
            {"earnings_inr",          rs.earnings_inr},
            {"solo_earnings_inr",     rs.solo_earnings_inr},
            {"earnings_increase",     rs.earnings_increase},
            {"earnings_increase_pct", rs.earnings_increase_pct},
            {"fuel_cost_inr",         rs.fuel_cost_inr},
            {"solo_fuel_cost_inr",    rs.solo_fuel_cost_inr},
            {"fuel_saved_inr",        rs.fuel_saved_inr},
            {"orders_per_hour",       rs.orders_per_hour}
        });
    }
    ja["rider_stats"] = rider_stats;
    return ja;
}

// ─── Build full state JSON ────────────────────────────────────
json ApiServer::buildState() {
    long long real_elapsed = nowMs() - start_real_ms_;
    long long sim_sec      = sim.simTimeSec();

    auto analytics = rm.buildAnalytics(
        sim_sec, real_elapsed,
        sim.totalOrdersGenerated(), 0
    );

    json state;
    state["sim_time_sec"]   = sim_sec;
    state["real_elapsed_ms"]= real_elapsed;
    state["sim_state"]      = (sim.state() == SimState::RUNNING) ? "RUNNING"
                            : (sim.state() == SimState::PAUSED)  ? "PAUSED"
                            : (sim.state() == SimState::TURBO)   ? "TURBO"
                            : "STOPPED";
    state["speed_multiplier"] = sim.config().speed_multiplier;

    // Orders
    json orders = json::array();
    for (const auto& o : om.allOrders())
        orders.push_back(orderToJson(o));
    state["orders"] = orders;

    // Riders
    json riders = json::array();
    for (const auto& r : rm.riders())
        riders.push_back(riderToJson(r));
    state["riders"] = riders;

    // Analytics
    state["analytics"] = analyticsToJson(analytics);

    return state;
}

// ─── Run HTTP Server ──────────────────────────────────────────
void ApiServer::run() {
    httplib::Server svr;

    // ── CORS middleware ──────────────────────────────────────
    auto setCORS = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    svr.Options(".*", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    // ── GET /api/graph ──────────────────────────────────
    // Graph is static (never mutated after startup) — no lock needed
    svr.Get("/api/graph", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.set_content(graphToJson().dump(), "application/json");
    });

    // ── GET /api/state (main polling endpoint) ────────────────
    // BUG-2 FIX: acquire shared mutex before reading sim/rider/order state
    svr.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = buildState().dump();
        }
        res.set_content(body, "application/json");
    });

    // ── GET /api/analytics ───────────────────────────────
    // BUG-2 FIX: lock before reading analytics
    svr.Get("/api/analytics", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            long long real_elapsed = nowMs() - start_real_ms_;
            auto analytics = rm.buildAnalytics(sim.simTimeSec(), real_elapsed,
                                               sim.totalOrdersGenerated(), 0);
            body = analyticsToJson(analytics).dump();
        }
        res.set_content(body, "application/json");
    });

    // ── POST /api/sim/start ──────────────────────────────────
    svr.Post("/api/sim/start", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        sim.start();
        res.set_content(R"({"status":"started"})", "application/json");
    });

    // ── POST /api/sim/stop ───────────────────────────────────
    svr.Post("/api/sim/stop", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        sim.stop();
        res.set_content(R"({"status":"stopped"})", "application/json");
    });

    // ── POST /api/sim/pause ──────────────────────────────────
    svr.Post("/api/sim/pause", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        sim.pause();
        res.set_content(R"({"status":"paused"})", "application/json");
    });

    // ── POST /api/sim/resume ─────────────────────────────────
    svr.Post("/api/sim/resume", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        sim.resume();
        res.set_content(R"({"status":"resumed"})", "application/json");
    });

    // ── POST /api/sim/speed  { "multiplier": 5.0 } ──────────
    svr.Post("/api/sim/speed", [&](const httplib::Request& req, httplib::Response& res) {
        setCORS(res);
        try {
            auto j = json::parse(req.body);
            double mult = j.value("multiplier", 1.0);
            sim.setSpeed(mult);
            res.set_content(
                json{{"status","ok"},{"multiplier",mult}}.dump(),
                "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid body"})", "application/json");
        }
    });

    // ── POST /api/sim/turbo  { "hours": 8 } ─────────────────
    svr.Post("/api/sim/turbo", [&](const httplib::Request& req, httplib::Response& res) {
        setCORS(res);
        try {
            auto j    = json::parse(req.body);
            int hours = j.value("hours", 8);
            int count = sim.turboSimulate(hours);
            res.set_content(
                json{{"status","done"},{"orders_generated",count},{"hours",hours}}.dump(),
                "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid body"})", "application/json");
        }
    });

    // ── POST /api/sim/reset ──────────────────────────────
    // BUG-1 FIX: actually resets all state — frees batch memory, clears orders
    svr.Post("/api/sim/reset", [&](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        std::lock_guard<std::mutex> lock(mutex_);
        sim.stop();
        rm.reset();           // free in-flight batch memory, reset stats
        om.resetOrders();     // clear all orders and pending queue
        start_real_ms_ = nowMs();
        res.set_content(R"({"status":"reset"})", "application/json");
    });

    // NOTE: /api/sim/order removed (BUG-10: called sim.tick() without mutex)

    std::cout << "[API] Server running on http://localhost:" << port_ << "\n";
    std::cout << "[API] Frontend should connect to: http://localhost:" << port_ << "/api/state\n";
    svr.listen("0.0.0.0", port_);
}
