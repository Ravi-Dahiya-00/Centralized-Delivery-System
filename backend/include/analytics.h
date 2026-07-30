#pragma once
#include <vector>
#include <string>

/*
 * Analytics Tracker
 * ─────────────────
 * Tracks all performance metrics for the management dashboard.
 * Computes both per-rider and system-wide statistics.
 *
 * KEY METRIC DEFINITIONS:
 *  - earnings_inr        : Actual earnings (base pay + distance bonus for all delivered orders)
 *  - solo_earnings_inr   : What earnings WOULD have been if every order was solo (no batching)
 *  - earnings_increase   : earnings_inr - solo_earnings_inr  (the bonus from batching)
 *  - distance_saved_m    : (sum of solo distances) - (actual total distance traveled)
 *  - fuel_cost_saved_inr : distance_saved_m * FUEL_RATE / 1000
 *
 * EARNING MODEL:
 *  - Per order base pay: ₹35
 *  - Distance bonus: ₹2 per km (actual distance traveled in batch for that order)
 *  - Batching means more orders per hour → more base pays earned
 *  - Solo model: same ₹35 + ₹2/km but rider makes fewer orders/hour
 */

struct RiderStats {
    int    rider_id;
    std::string rider_name;

    int    orders_delivered    = 0;
    int    batches_completed   = 0;
    int    solo_orders         = 0;    // orders delivered alone (not batched)
    int    batched_orders      = 0;    // orders delivered in a batch

    double actual_distance_km  = 0.0;
    double solo_distance_km    = 0.0;
    double distance_saved_km   = 0.0;

    double earnings_inr        = 0.0;
    double solo_earnings_inr   = 0.0;
    double earnings_increase   = 0.0;  // earnings_inr - solo_earnings_inr
    double earnings_increase_pct = 0.0;

    double fuel_cost_inr       = 0.0;
    double solo_fuel_cost_inr  = 0.0;
    double fuel_saved_inr      = 0.0;

    double avg_delivery_time_min = 0.0;
    double orders_per_hour       = 0.0;
};

struct SystemAnalytics {
    // ── Totals ──────────────────────────────────────────────
    int    total_orders_placed    = 0;
    int    total_orders_delivered = 0;
    int    total_batches          = 0;
    int    total_batched_orders   = 0;
    int    total_solo_orders      = 0;
    int    total_cancelled        = 0;

    double total_distance_km      = 0.0;
    double total_solo_distance_km = 0.0;
    double total_distance_saved_km = 0.0;
    double avg_distance_saved_pct  = 0.0;  // % reduction per batch

    double total_earnings_inr     = 0.0;
    double total_solo_earnings_inr= 0.0;
    double total_earnings_increase= 0.0;   // ₹ extra earned due to batching
    double earnings_increase_pct  = 0.0;   // % increase

    double total_fuel_cost_inr    = 0.0;
    double total_fuel_saved_inr   = 0.0;   // ₹ saved on fuel across all riders

    double batch_rate_pct         = 0.0;   // % of orders that were batched
    double avg_delivery_time_min  = 0.0;
    double system_orders_per_hour = 0.0;

    long long simulation_time_sec = 0;     // current sim time elapsed
    long long real_time_elapsed_ms= 0;

    // ── Per-Rider Stats ──────────────────────────────────────
    std::vector<RiderStats> rider_stats;
};
