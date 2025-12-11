#pragma once

#include <string>
#include <unordered_map>

namespace qf {

class StrategyConfigurator {
public:
    using ParamMap = std::unordered_map<std::string, std::string>;

    // 读取策略参数配置。
    bool load(const std::string& path);
    // 按策略 ID 获取参数键值对。
    ParamMap get_params(const std::string& strategy_id) const;

private:
    std::unordered_map<std::string, ParamMap> configs_;
};

} // namespace qf

