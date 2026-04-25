#include "smartquant/alpha/signal_engine.hpp"

#include <cmath>

namespace sq {

SignalEngine::SignalEngine(double thresh_delta, double min_cancel_z)
    : thresh_delta_(thresh_delta), min_cancel_z_(min_cancel_z) {}

void SignalEngine::reset() noexcept {
    delta_.reset();
    cancel_.reset();
    last_signal_ = Signal{};
}

bool SignalEngine::on_event(const LOBEvent& ev, const OrderBook& book) {
    // Keep delta factor's midprice current
    const Price bid = book.best_bid();
    const Price ask = book.best_ask();
    if (bid > 0 && ask > 0)
        delta_.set_midprice((bid + ask) / 2);

    delta_.on_event(ev);
    cancel_.on_event(ev);

    // Adaptive threshold: use 95th-percentile; fall back to min_cancel_z_
    const double thresh_cancel =
        std::max(min_cancel_z_, cancel_.dynamic_threshold());

    const double a_delta = delta_.value();
    const double z_b     = cancel_.z_bid();
    const double z_a     = cancel_.z_ask();

    Direction dir = Direction::None;

    if (a_delta > thresh_delta_ && z_a > thresh_cancel) {
        // Ask side collapsing while strong buying pressure → BUY
        dir = Direction::Buy;
    } else if (a_delta < -thresh_delta_ && z_b > thresh_cancel) {
        // Bid side collapsing while strong selling pressure → SELL
        dir = Direction::Sell;
    }

    if (dir == Direction::None) return false;

    last_signal_ = Signal{
        .trigger_time_ns = ev.timestamp_ns,
        .l1_price        = (dir == Direction::Buy) ? ask : bid,
        .delta_val       = a_delta,
        .cancel_z        = (dir == Direction::Buy) ? z_a : z_b,
        .direction       = dir
    };
    return true;
}

}  // namespace sq
