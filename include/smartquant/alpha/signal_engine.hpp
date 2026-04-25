#pragma once

#include <cstdint>

#include "smartquant/common/types.hpp"
#include "smartquant/alpha/delta_factor.hpp"
#include "smartquant/alpha/cancel_factor.hpp"
#include "smartquant/md/order_book.hpp"

namespace sq {

// Dual-factor signal engine.
//
// Combines DeltaFactor (tick imbalance acceleration) and CancelFactor
// (cancellation rate z-score) to produce a directional Signal:
//
//   BUY  when: a_Δ > +thresh_delta  AND  z_ask > thresh_cancel  (Ask collapse)
//   SELL when: a_Δ < -thresh_delta  AND  z_bid > thresh_cancel  (Bid collapse)
//
// thresh_cancel is adaptive: it tracks the 95th-percentile of recent z-scores
// over a 30-second window (via CancelFactor::dynamic_threshold()).
class SignalEngine {
public:
    // thresh_delta: minimum |a_Δ| to consider (calibrate via backtest)
    // min_cancel_z: minimum z-score to consider; used as floor when the
    //               adaptive threshold hasn't warmed up yet
    explicit SignalEngine(double thresh_delta  = 50.0,
                          double min_cancel_z  = 1.5);

    // Feed one LOBEvent to both factors and re-evaluate.
    // Returns true if a new signal was generated (check last_signal()).
    bool on_event(const LOBEvent& ev, const OrderBook& book);

    [[nodiscard]] const Signal& last_signal() const noexcept { return last_signal_; }

    void set_thresh_delta(double v) noexcept { thresh_delta_ = v; }
    void set_min_cancel_z(double v) noexcept { min_cancel_z_ = v; }

    void reset() noexcept;

private:
    DeltaFactor  delta_;
    CancelFactor cancel_;

    double thresh_delta_;
    double min_cancel_z_;

    Signal last_signal_;
};

}  // namespace sq
