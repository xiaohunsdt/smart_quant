#pragma once

#include "qf/strategy/strategy_base.hpp"
#include <string>

namespace qf {

// 示例策略：收到 Tick 后直接发出买入信号。
class SampleStrategy : public StrategyBase {
public:
    explicit SampleStrategy(std::string id);
    std::string id() const override;
    void on_init() override;
    void on_tick(const TickData& tick) override;
    void on_stop() override;

private:
    std::string id_;
};

} // namespace qf

