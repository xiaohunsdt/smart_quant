#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "qf/strategy/strategy_base.hpp"

namespace qf {

class StrategyFactory {
public:
    using Creator = std::function<std::unique_ptr<StrategyBase>()>;
    static StrategyFactory& instance();

    // 注册策略类型与构造器。
    void register_creator(const std::string& type, Creator creator);
    // 根据类型创建策略实例。
    std::unique_ptr<StrategyBase> create(const std::string& type) const;

private:
    StrategyFactory() = default;
    std::unordered_map<std::string, Creator> creators_;
};

} // namespace qf

