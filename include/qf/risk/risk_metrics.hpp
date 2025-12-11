#pragma once

#include <unordered_map>
#include <string>
#include "qf/core/types.hpp"

namespace qf {

class RiskMetrics {
public:
    // 更新某合约的风险快照。
    void update(const PositionData& pos);
    // 汇总总仓位。
    double total_position() const;
    // 查询单合约仓位。
    double symbol_position(const std::string& symbol) const;

private:
    std::unordered_map<std::string, PositionData> snapshot_;
};

} // namespace qf

