#include "../include/rider.h"

std::string riderStatusToString(RiderStatus s) {
    switch (s) {
        case RiderStatus::IDLE:                return "IDLE";
        case RiderStatus::MOVING_TO_PICKUP:    return "MOVING_TO_PICKUP";
        case RiderStatus::MOVING_TO_DELIVERY:  return "MOVING_TO_DELIVERY";
        // PICKING_UP and DELIVERING removed (Bug 14 fix) — never used
        default:                               return "UNKNOWN";
    }
}
