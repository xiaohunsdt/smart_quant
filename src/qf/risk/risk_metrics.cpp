#include "qf/risk/risk_metrics.hpp"
#include "qf/infrastructure/logger.hpp"

namespace qf {

void RiskMetrics::update(const PositionData& pos) {
    // 覆盖最新快照，实际可追加历史用于趋势分析。
    snapshot_[pos.symbol] = pos;
    Logger::instance().debug(
        "风控指标更新: 合约={} 多头={} 空头={} 浮动盈亏={}", pos.symbol, pos.long_qty, pos.short_qty, pos.unrealized_pnl);
}

double RiskMetrics::total_position() const {
    double total = 0.0;
    for (auto& kv : snapshot_) {
        total += kv.second.long_qty + kv.second.short_qty;
    }
    return total;
}

double RiskMetrics::symbol_position(const std::string& symbol) const {
    auto it = snapshot_.find(symbol);
    if (it != snapshot_.end()) {
        return it->second.long_qty + it->second.short_qty;
    }
    return 0.0;
}

} // namespace qf

