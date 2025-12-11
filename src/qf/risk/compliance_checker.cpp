#include "qf/risk/compliance_checker.hpp"
#include "qf/infrastructure/logger.hpp"
#include <algorithm>

namespace qf {

void ComplianceChecker::set_symbol_whitelist(const std::vector<std::string>& symbols) {
    whitelist_ = symbols;
}

bool ComplianceChecker::allowed(const Signal& signal) const {
    if (whitelist_.empty()) return true;
    // 仅允许白名单合约。
    bool ok = std::find(whitelist_.begin(), whitelist_.end(), signal.symbol) != whitelist_.end();
    if (!ok) {
        Logger::instance().warn("合规检查拒绝: 合约={} 策略={}", signal.symbol, signal.strategy_id);
    }
    return ok;
}

} // namespace qf

