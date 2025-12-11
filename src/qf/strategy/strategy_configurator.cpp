#include "qf/strategy/strategy_configurator.hpp"
#include <fstream>

namespace qf {

bool StrategyConfigurator::load(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        // simple INI like: strategy.id.key=value
        auto first_dot = line.find('.');
        auto eq = line.find('=');
        if (first_dot == std::string::npos || eq == std::string::npos) continue;
        auto strategy = line.substr(0, first_dot);
        auto key = line.substr(first_dot + 1, eq - first_dot - 1);
        auto val = line.substr(eq + 1);
        // 将参数写入对应策略的参数表。
        configs_[strategy][key] = val;
    }
    return true;
}

StrategyConfigurator::ParamMap StrategyConfigurator::get_params(const std::string& strategy_id) const {
    auto it = configs_.find(strategy_id);
    if (it != configs_.end()) {
        return it->second;
    }
    return {};
}

} // namespace qf

