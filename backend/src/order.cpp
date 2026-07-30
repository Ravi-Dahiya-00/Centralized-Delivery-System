#include "../include/order.h"

std::string platformToString(Platform p) {
    return p == Platform::ZOMATO ? "ZOMATO" : "SWIGGY";
}

std::string orderStatusToString(OrderStatus s) {
    switch (s) {
        case OrderStatus::PENDING:    return "PENDING";
        case OrderStatus::ASSIGNED:   return "ASSIGNED";
        case OrderStatus::PICKED_UP:  return "PICKED_UP";
        case OrderStatus::DELIVERED:  return "DELIVERED";
        case OrderStatus::CANCELLED:  return "CANCELLED";
        default:                      return "UNKNOWN";
    }
}
