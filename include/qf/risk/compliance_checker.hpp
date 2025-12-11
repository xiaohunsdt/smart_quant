#pragma once

#include <vector>
#include <string>
#include "qf/core/types.hpp"

namespace qf {

class ComplianceChecker {
public:
    // 设置合约白名单。
    void set_symbol_whitelist(const std::vector<std::string>& symbols);
    // 校验信号是否允许交易。
    bool allowed(const Signal& signal) const;

private:
    std::vector<std::string> whitelist_;
};

} // namespace qf

