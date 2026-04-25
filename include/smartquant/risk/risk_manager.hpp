#pragma once

#include <cstdint>
#include <functional>

#include "smartquant/common/types.hpp"

namespace sq {

// Risk management: guards every signal and every fill.
//
// Rules (all configurable):
//  • Max 1 open position at a time
//  • Daily loss limit (fuse)
//  • Cooldown period after excessive slippage
//  • Position timeout → forced market close
class RiskManager {
public:
    struct Config {
        double   max_daily_loss_usd     = 50.0;   // fuse
        double   tick_size_usd          = 1.0;    // 1 tick = 1 USD for 0.1 lot
        int      cooldown_consec_slips  = 3;      // consecutive over-slippage count
        double   cooldown_slip_ticks    = 2.0;    // threshold per trade
        double   single_slip_abort_ticks= 5.0;    // single trade hard abort
        uint64_t cooldown_duration_ns   = 60'000'000'000ULL;  // 60 s
        uint64_t position_timeout_ns    = 5'000'000'000ULL;   //  5 s
    };

    using ForceCloseCallback = std::function<void(const Order&)>;

    explicit RiskManager(const Config& cfg);
    RiskManager() : RiskManager(Config{}) {}

    // Called before sending an order.  Returns false if trading is blocked.
    [[nodiscard]] bool check_signal(const Signal& sig, uint64_t now_ns) noexcept;

    // Called when an IOC order has been sent (but not yet filled).
    void on_order_sent(const Order& order, uint64_t now_ns) noexcept;

    // Called when an ExecutionReport (fill/partial) arrives.
    void on_fill(const Order& order, uint64_t now_ns) noexcept;

    // Called when an order is rejected or cancelled without fill.
    void on_reject(const Order& order) noexcept;

    // Must be called periodically (e.g. once per event loop tick).
    // Triggers position timeout if needed and invokes the callback.
    void on_timer(uint64_t now_ns, const ForceCloseCallback& force_close);

    // End-of-day reset (call at session start / midnight)
    void reset_daily() noexcept;

    // ── State queries ─────────────────────────────────────────────────────────
    [[nodiscard]] bool     has_open_position() const noexcept { return has_position_; }
    [[nodiscard]] double   daily_pnl_usd()     const noexcept { return daily_pnl_; }
    [[nodiscard]] bool     is_fused()          const noexcept { return fused_; }
    [[nodiscard]] bool     in_cooldown(uint64_t now_ns) const noexcept;
    [[nodiscard]] uint64_t total_trades()      const noexcept { return total_trades_; }

private:
    double slippage_ticks(const Order& o) const noexcept;

    Config cfg_;

    bool     has_position_{false};
    Order    open_order_{};
    uint64_t position_open_ns_{0};

    double   daily_pnl_{0.0};
    bool     fused_{false};

    int      consec_slip_count_{0};
    uint64_t cooldown_start_ns_{0};

    uint64_t total_trades_{0};
};

}  // namespace sq
