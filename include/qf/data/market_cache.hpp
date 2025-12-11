#pragma once

#include <unordered_map>
#include <deque>
#include <mutex>
#include "qf/core/types.hpp"

namespace qf {

class MarketCache {
public:
    explicit MarketCache(std::size_t max_ticks = 3600);
    // 存入最新行情，自动淘汰超出窗口的数据。
    void put(const TickData& tick);
    // 按合约查询缓存窗口内的行情序列。
    std::vector<TickData> query(const std::string& symbol) const;

private:
    std::size_t max_ticks_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::deque<TickData>> cache_;
};

} // namespace qf

