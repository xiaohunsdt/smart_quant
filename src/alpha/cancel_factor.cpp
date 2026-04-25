#include "smartquant/alpha/cancel_factor.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace sq {

void CancelFactor::reset() noexcept {
    for (auto& b : buckets_) b = {};
    head_ = count_ = 0;
    z_bid_ = z_ask_ = 0.0;
    recent_z_bid_.clear();
    recent_z_ask_.clear();
    recent_ts_.clear();
}

void CancelFactor::advance_time(uint64_t now_ns) {
    if (count_ == 0) {
        buckets_[0] = {now_ns, 0.0, 0.0};
        head_  = 0;
        count_ = 1;
        return;
    }

    const uint64_t bucket_start = buckets_[head_].ts_ns;
    if (now_ns < bucket_start + kBucketNs)
        return;

    const std::size_t steps = static_cast<std::size_t>(
        (now_ns - bucket_start) / kBucketNs);
    const std::size_t to_add = std::min(steps, kMaxBuckets);

    for (std::size_t i = 0; i < to_add; ++i) {
        head_ = (head_ + 1) % kMaxBuckets;
        buckets_[head_] = {bucket_start + (i + 1) * kBucketNs, 0.0, 0.0};
        if (count_ < kMaxBuckets) ++count_;
    }
}

void CancelFactor::on_event(const LOBEvent& ev) {
    advance_time(ev.timestamp_ns);

    if (ev.action == MDUpdateAction::Delete) {
        switch (ev.side) {
        case MDEntryType::Bid:
            buckets_[head_].bid_cancels += 1.0;
            break;
        case MDEntryType::Offer:
            buckets_[head_].ask_cancels += 1.0;
            break;
        default:
            break;
        }
    }

    recompute_zscores();
}

void CancelFactor::recompute_zscores() {
    if (count_ < 2) return;

    const uint64_t now_ns = buckets_[head_].ts_ns;
    const uint64_t cutoff = (now_ns >= kWindowNs) ? (now_ns - kWindowNs) : 0;

    // Collect samples within the 1-second window
    double sum_bid = 0.0, sum_ask = 0.0;
    double sq_bid  = 0.0, sq_ask  = 0.0;
    std::size_t n = 0;

    for (std::size_t i = 0; i < count_; ++i) {
        const std::size_t idx = (head_ + kMaxBuckets - i) % kMaxBuckets;
        if (buckets_[idx].ts_ns < cutoff) break;
        sum_bid += buckets_[idx].bid_cancels;
        sum_ask += buckets_[idx].ask_cancels;
        sq_bid  += buckets_[idx].bid_cancels * buckets_[idx].bid_cancels;
        sq_ask  += buckets_[idx].ask_cancels * buckets_[idx].ask_cancels;
        ++n;
    }

    if (n < 2) return;

    const double fn   = static_cast<double>(n);
    const double mu_b = sum_bid / fn;
    const double mu_a = sum_ask / fn;

    const double var_b = sq_bid / fn - mu_b * mu_b;
    const double var_a = sq_ask / fn - mu_a * mu_a;

    const double sigma_b = (var_b > 0.0) ? std::sqrt(var_b) : 1.0;
    const double sigma_a = (var_a > 0.0) ? std::sqrt(var_a) : 1.0;

    // Current bucket counts
    const double c_bid = buckets_[head_].bid_cancels;
    const double c_ask = buckets_[head_].ask_cancels;

    z_bid_ = (c_bid - mu_b) / sigma_b;
    z_ask_ = (c_ask - mu_a) / sigma_a;

    // Record for 30-second dynamic threshold
    recent_z_bid_.push_back(z_bid_);
    recent_z_ask_.push_back(z_ask_);
    recent_ts_.push_back(now_ns);

    // Expire entries older than kQuantileNs
    const uint64_t q_cutoff = (now_ns >= kQuantileNs) ? (now_ns - kQuantileNs) : 0;
    while (!recent_ts_.empty() && recent_ts_.front() < q_cutoff) {
        recent_ts_.pop_front();
        recent_z_bid_.pop_front();
        recent_z_ask_.pop_front();
    }
}

double CancelFactor::dynamic_threshold() const noexcept {
    // 95th percentile of recent max(z_bid, z_ask) values
    if (recent_z_bid_.empty()) return 2.0;  // fallback

    std::vector<double> combined;
    combined.reserve(recent_z_bid_.size());
    for (std::size_t i = 0; i < recent_z_bid_.size(); ++i)
        combined.push_back(std::max(recent_z_bid_[i], recent_z_ask_[i]));

    std::sort(combined.begin(), combined.end());
    const std::size_t idx =
        static_cast<std::size_t>(0.95 * static_cast<double>(combined.size() - 1));
    return combined[idx];
}

}  // namespace sq
