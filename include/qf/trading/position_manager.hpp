#pragma once

#include <unordered_map>
#include <mutex>
#include "qf/core/types.hpp"

namespace qf {

class PositionManager {
public:
    // 根据成交回报更新持仓。
    void apply_trade(const TradeData& trade);
    // 查询单个合约持仓。
    PositionData get(const std::string& symbol) const;
    // 获取全部持仓快照。
    std::vector<PositionData> list() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PositionData> positions_;
};

} // namespace qf

