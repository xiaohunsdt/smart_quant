#include "qf/strategy/strategy_factory.hpp"

namespace qf {

StrategyFactory& StrategyFactory::instance() {
    static StrategyFactory inst;
    return inst;
}

void StrategyFactory::register_creator(const std::string& type, Creator creator) {
    creators_[type] = std::move(creator);
}

std::unique_ptr<StrategyBase> StrategyFactory::create(const std::string& type) const {
    auto it = creators_.find(type);
    if (it == creators_.end()) return nullptr;
    // 调用已注册的构造器创建实例。
    return (it->second)();
}

} // namespace qf

