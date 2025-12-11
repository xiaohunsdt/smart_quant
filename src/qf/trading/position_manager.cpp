#include "qf/trading/position_manager.hpp"

namespace qf {

void PositionManager::apply_trade(const TradeData& trade) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& pos = positions_[trade.symbol];
    // Simple netting logic: buy positive, sell negative assumed from trade_id prefix
    double qty = trade.volume;
    pos.symbol = trade.symbol;
    // 占位实现：仅累加多头仓位。
    pos.long_qty += qty;
    pos.avg_price = trade.price; // naive update for placeholder
}

PositionData PositionManager::get(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(symbol);
    if (it != positions_.end()) return it->second;
    return {};
}

std::vector<PositionData> PositionManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PositionData> out;
    out.reserve(positions_.size());
    for (auto& kv : positions_) out.push_back(kv.second);
    return out;
}

} // namespace qf

