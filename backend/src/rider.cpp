#include "../include/rider.h"

std::string riderStatusToString(RiderStatus s) {
    switch (s) {
        case RiderStatus::IDLE:                return "IDLE";
        case RiderStatus::MOVING_TO_PICKUP:    return "MOVING_TO_PICKUP";
        case RiderStatus::PICKING_UP:          return "PICKING_UP";
        case RiderStatus::MOVING_TO_DELIVERY:  return "MOVING_TO_DELIVERY";
        case RiderStatus::DELIVERING:          return "DELIVERING";
        default:                               return "UNKNOWN";
    }
}
