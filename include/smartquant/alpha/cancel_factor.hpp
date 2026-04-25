#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

#include "smartquant/common/types.hpp"

namespace sq {

// Cancellation Rate Velocity factor.
//
// Monitors MDUpdateAction=Delete events separately for the Bid and Ask sides.
// Within a 10 ms bucket, counts Delete events.  Over a rolling 1-second window
// it maintains the mean μ and standard deviation σ of these counts, then
// produces a z-score per side:
//
//   z_side(t) = (c(t) − μ_side) / σ_side
//
// Direction semantics (from dep.md):
//   z_bid > thresh  → Bid collapse → bearish (SELL signal contributor)
//   z_ask > thresh  → Ask collapse → bullish (BUY  signal contributor)
//
// Dynamic threshold: the class maintains a 30-second sliding quantile so the
// threshold adapts to intraday volatility regimes.
class CancelFactor {
public:
    static constexpr uint64_t kBucketNs    = 10'000'000ULL;   //  10 ms
    static constexpr uint64_t kWindowNs    = 1'000'000'000ULL; //   1 s  (for z-score)
    static constexpr uint64_t kQuantileNs  = 30'000'000'000ULL;// 30 s  (for threshold)
    static constexpr std::size_t kMaxBuckets = 200;  // 2 s of 10ms buckets

    CancelFactor() = default;

    // Feed one LOBEvent.  Only MDUpdateAction::Delete events are counted.
    void on_event(const LOBEvent& ev);

    // z-scores (positive = abnormally high cancellation rate)
    [[nodiscard]] double z_bid() const noexcept { return z_bid_; }
    [[nodiscard]] double z_ask() const noexcept { return z_ask_; }

    // 95th-percentile dynamic threshold over the past 30 seconds
    [[nodiscard]] double dynamic_threshold() const noexcept;

    void reset() noexcept;

private:
    struct Bucket {
        uint64_t ts_ns{0};
        double   bid_cancels{0.0};
        double   ask_cancels{0.0};
    };

    void advance_time(uint64_t now_ns);
    void recompute_zscores();

    // Fixed-size circular buffer for buckets
    std::array<Bucket, kMaxBuckets> buckets_{};
    std::size_t head_{0};
    std::size_t count_{0};

    double z_bid_{0.0};
    double z_ask_{0.0};

    // Rolling z-score values kept for the 30-second dynamic threshold
    std::deque<double> recent_z_bid_;
    std::deque<double> recent_z_ask_;
    std::deque<uint64_t> recent_ts_;
};

}  // namespace sq
