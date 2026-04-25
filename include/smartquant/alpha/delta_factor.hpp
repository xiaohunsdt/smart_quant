#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "smartquant/common/types.hpp"

namespace sq {

// Tick Delta Velocity factor.
//
// Computes the second-order derivative of the net buy/sell volume imbalance
// over a configurable sliding window:
//
//   Δ(t)   = Σ buy_vol(t-W..t)  -  Σ sell_vol(t-W..t)
//   a_Δ(t) = Δ(t) - 2·Δ(t-1) + Δ(t-2)   (discrete second derivative)
//
// Trades with price >= midpoint are classified as buyer-initiated;
// trades below midpoint are seller-initiated.
//
// Feed every incoming LOBEvent to on_event().  After each call, query value()
// to obtain the current a_Δ.
class DeltaFactor {
public:
    // window_ns: sliding window duration in nanoseconds (default 100 ms)
    // bucket_ns: time bucket size for the discrete approximation (default 10 ms)
    static constexpr uint64_t kDefaultWindowNs = 100'000'000ULL;  // 100 ms
    static constexpr uint64_t kDefaultBucketNs =  10'000'000ULL;  //  10 ms
    static constexpr std::size_t kMaxBuckets   = 64;

    explicit DeltaFactor(uint64_t window_ns = kDefaultWindowNs,
                         uint64_t bucket_ns = kDefaultBucketNs);

    // Feed one LOBEvent.  Only Trade events contribute to delta; Bid/Ask
    // events are ignored (but still advance internal timekeeping).
    void on_event(const LOBEvent& ev);

    // Current a_Δ (second derivative of net imbalance, in qty ticks).
    [[nodiscard]] double value() const noexcept { return acceleration_; }

    // Raw Δ(t) (net imbalance over window, for single-factor comparison)
    [[nodiscard]] double net_delta() const noexcept { return net_delta_; }

    // Reset all state
    void reset() noexcept;

    void set_midprice(Price mid) noexcept { midprice_ = mid; }

private:
    struct Bucket {
        uint64_t ts_ns{0};       // bucket start timestamp
        double   buy_vol{0.0};
        double   sell_vol{0.0};
    };

    void advance_time(uint64_t now_ns);
    void recompute();

    uint64_t window_ns_;
    uint64_t bucket_ns_;
    std::size_t num_buckets_;

    std::array<Bucket, kMaxBuckets> buckets_{};
    std::size_t head_{0};   // index of the current (most recent) bucket
    std::size_t count_{0};  // number of valid buckets

    Price  midprice_{0};
    double net_delta_{0.0};
    double prev_delta_{0.0};
    double prev_prev_delta_{0.0};
    double acceleration_{0.0};
};

}  // namespace sq
