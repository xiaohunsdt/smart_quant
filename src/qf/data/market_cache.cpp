#include "qf/data/market_cache.hpp"

namespace qf {

MarketCache::MarketCache(std::size_t max_ticks) : max_ticks_(max_ticks) {}

void MarketCache::put(const TickData& tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& dq = cache_[tick.symbol];
    dq.push_back(tick);
    // 超出窗口大小时淘汰最旧数据。
    while (dq.size() > max_ticks_) {
        dq.pop_front();
    }
}

std::vector<TickData> MarketCache::query(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TickData> result;
    auto it = cache_.find(symbol);
    if (it != cache_.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

} // namespace qf

