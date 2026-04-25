#include "smartquant/alpha/delta_factor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sq {

DeltaFactor::DeltaFactor(uint64_t window_ns, uint64_t bucket_ns)
    : window_ns_(window_ns), bucket_ns_(bucket_ns) {
    if (bucket_ns == 0)
        throw std::invalid_argument("DeltaFactor: bucket_ns must be > 0");
    num_buckets_ = std::min(
        static_cast<std::size_t>((window_ns + bucket_ns - 1) / bucket_ns) + 1,
        kMaxBuckets);
}

void DeltaFactor::reset() noexcept {
    for (auto& b : buckets_) b = {};
    head_          = 0;
    count_         = 0;
    net_delta_     = 0.0;
    prev_delta_    = 0.0;
    prev_prev_delta_ = 0.0;
    acceleration_  = 0.0;
}

void DeltaFactor::advance_time(uint64_t now_ns) {
    if (count_ == 0) {
        // First event: initialise the first bucket
        buckets_[0] = {now_ns, 0.0, 0.0};
        head_  = 0;
        count_ = 1;
        return;
    }

    const uint64_t bucket_start = buckets_[head_].ts_ns;
    if (now_ns < bucket_start + bucket_ns_)
        return;  // still in current bucket

    // How many new buckets do we need?
    const std::size_t new_count = static_cast<std::size_t>(
        (now_ns - bucket_start) / bucket_ns_);

    const std::size_t to_add = std::min(new_count, num_buckets_);

    for (std::size_t i = 0; i < to_add; ++i) {
        head_         = (head_ + 1) % kMaxBuckets;
        buckets_[head_] = {bucket_start + (i + 1) * bucket_ns_, 0.0, 0.0};
        if (count_ < kMaxBuckets) ++count_;
    }
}

void DeltaFactor::on_event(const LOBEvent& ev) {
    advance_time(ev.timestamp_ns);

    if (ev.side == MDEntryType::Trade && ev.qty > 0) {
        // Classify by comparing trade price to current midprice
        if (midprice_ <= 0 || ev.price >= midprice_)
            buckets_[head_].buy_vol  += static_cast<double>(ev.qty);
        else
            buckets_[head_].sell_vol += static_cast<double>(ev.qty);
    }

    recompute();
}

void DeltaFactor::recompute() {
    // Sum all buckets within the window
    const uint64_t cutoff =
        (buckets_[head_].ts_ns >= window_ns_)
            ? buckets_[head_].ts_ns - window_ns_
            : 0;

    double buy_sum = 0.0, sell_sum = 0.0;
    for (std::size_t i = 0; i < count_; ++i) {
        const std::size_t idx = (head_ + kMaxBuckets - i) % kMaxBuckets;
        if (buckets_[idx].ts_ns < cutoff) break;
        buy_sum  += buckets_[idx].buy_vol;
        sell_sum += buckets_[idx].sell_vol;
    }

    prev_prev_delta_ = prev_delta_;
    prev_delta_      = net_delta_;
    net_delta_       = buy_sum - sell_sum;

    // Discrete second derivative
    acceleration_ = net_delta_ - 2.0 * prev_delta_ + prev_prev_delta_;
}

}  // namespace sq
