#pragma once

#include <cstdint>
#include <string>

namespace sq {

// ── Fundamental price/size types ──────────────────────────────────────────────
// Prices stored as integer ticks (price * 100 for XAUUSD, 2 decimal places)
using Price = int64_t;
using Qty   = int64_t;

static constexpr Price kPriceScale = 100;  // 1 USD = 100 price units

inline Price to_price(double d)   { return static_cast<Price>(d * kPriceScale + 0.5); }
inline double from_price(Price p) { return static_cast<double>(p) / kPriceScale; }

// ── LOB event ─────────────────────────────────────────────────────────────────
enum class MDUpdateAction : uint8_t {
    New    = 0,
    Change = 1,
    Delete = 2
};

enum class MDEntryType : uint8_t {
    Bid   = 0,
    Offer = 1,
    Trade = 2
};

struct LOBEvent {
    uint64_t      timestamp_ns;
    Price         price;
    Qty           qty;
    MDUpdateAction action;
    MDEntryType   side;
};

// ── Trade signal ──────────────────────────────────────────────────────────────
enum class Direction : uint8_t { None = 0, Buy = 1, Sell = 2 };

struct Signal {
    uint64_t  trigger_time_ns{0};
    Price     l1_price{0};       // best bid (sell) or best ask (buy) at signal time
    double    delta_val{0.0};    // a_Δ value that triggered
    double    cancel_z{0.0};     // z-score value that triggered
    Direction direction{Direction::None};
};

// ── Order ─────────────────────────────────────────────────────────────────────
enum class OrderStatus : uint8_t {
    Pending  = 0,
    Sent     = 1,
    Filled   = 2,
    Rejected = 3,
    Cancelled= 4
};

enum class Side : uint8_t { Buy = 0, Sell = 1 };

struct Order {
    char        clordid[32]{};
    uint64_t    sent_ts_ns{0};
    uint64_t    fill_ts_ns{0};
    Price       signal_l1{0};   // L1 price at signal time (for slippage calc)
    Price       fill_price{0};
    Qty         qty{0};
    Side        side{Side::Buy};
    OrderStatus status{OrderStatus::Pending};
};

}  // namespace sq
