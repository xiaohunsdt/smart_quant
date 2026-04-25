#include "smartquant/risk/risk_manager.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

namespace sq {

RiskManager::RiskManager(const Config& cfg) : cfg_(cfg) {}

bool RiskManager::in_cooldown(uint64_t now_ns) const noexcept {
    if (cooldown_start_ns_ == 0) return false;
    return (now_ns - cooldown_start_ns_) < cfg_.cooldown_duration_ns;
}

bool RiskManager::check_signal(const Signal& /*sig*/, uint64_t now_ns) noexcept {
    if (fused_) {
        spdlog::warn("RiskManager: trading fused — daily loss limit hit");
        return false;
    }
    if (has_position_) {
        spdlog::debug("RiskManager: already has open position, skipping signal");
        return false;
    }
    if (in_cooldown(now_ns)) {
        spdlog::debug("RiskManager: in cooldown period");
        return false;
    }
    return true;
}

void RiskManager::on_order_sent(const Order& order, uint64_t now_ns) noexcept {
    has_position_     = true;
    open_order_       = order;
    position_open_ns_ = now_ns;
}

double RiskManager::slippage_ticks(const Order& o) const noexcept {
    if (o.fill_price == 0 || o.signal_l1 == 0) return 0.0;
    const double diff = std::abs(
        static_cast<double>(o.fill_price - o.signal_l1));
    // kPriceScale = 100 → 1 tick = 1 USD/0.1lot stored as 100 units
    return diff / static_cast<double>(kPriceScale);
}

void RiskManager::on_fill(const Order& order, uint64_t now_ns) noexcept {
    has_position_ = false;
    ++total_trades_;

    // PnL: positive for profit.
    // For a BUY: profit = fill_price < signal_l1 + expected_gain (simplified)
    // Here we just track slippage cost as negative PnL adjustment.
    const double slip = slippage_ticks(order);
    const double slip_cost = -slip * cfg_.tick_size_usd;
    daily_pnl_ += slip_cost;

    spdlog::info("Fill: slippage={:.2f} ticks  ({:.2f} USD)  daily_pnl={:.2f}",
                 slip, slip_cost, daily_pnl_);

    // Single trade hard abort
    if (slip > cfg_.single_slip_abort_ticks) {
        spdlog::error("RiskManager: single-trade slippage {:.2f} ticks > {:.2f} — "
                      "entering cooldown immediately",
                      slip, cfg_.single_slip_abort_ticks);
        cooldown_start_ns_ = now_ns;
        consec_slip_count_ = 0;
        return;
    }

    // Consecutive slippage counter
    if (slip > cfg_.cooldown_slip_ticks) {
        ++consec_slip_count_;
        spdlog::warn("RiskManager: consec slip count = {}", consec_slip_count_);
        if (consec_slip_count_ >= cfg_.cooldown_consec_slips) {
            spdlog::warn("RiskManager: {} consecutive over-slippage events — "
                         "entering 60s cooldown",
                         cfg_.cooldown_consec_slips);
            cooldown_start_ns_ = now_ns;
            consec_slip_count_ = 0;
        }
    } else {
        consec_slip_count_ = 0;
    }

    // Daily loss fuse
    if (daily_pnl_ < -cfg_.max_daily_loss_usd) {
        spdlog::error("RiskManager: daily loss limit reached ({:.2f} USD) — FUSED",
                      daily_pnl_);
        fused_ = true;
    }

    (void)now_ns;
}

void RiskManager::on_reject(const Order& /*order*/) noexcept {
    has_position_ = false;
}

void RiskManager::on_timer(uint64_t now_ns, const ForceCloseCallback& force_close) {
    if (!has_position_) return;
    if ((now_ns - position_open_ns_) >= cfg_.position_timeout_ns) {
        spdlog::warn("RiskManager: position timeout — forcing market close");
        force_close(open_order_);
    }
}

void RiskManager::reset_daily() noexcept {
    daily_pnl_         = 0.0;
    fused_             = false;
    consec_slip_count_ = 0;
    cooldown_start_ns_ = 0;
    has_position_      = false;
    total_trades_      = 0;
}

}  // namespace sq
