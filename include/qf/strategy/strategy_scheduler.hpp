#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include "qf/strategy/strategy_base.hpp"
#include "qf/infrastructure/thread_pool.hpp"

namespace qf {

class StrategyScheduler : public StrategyContext {
public:
    explicit StrategyScheduler(std::size_t workers = 2);

    // 注册策略并调用其初始化。
    void add_strategy(std::unique_ptr<StrategyBase> strategy);
    // 收到行情后分发给所有策略（线程池执行）。
    void on_tick(const TickData& tick);
    // 停止全部策略并关闭线程池。
    void stop_all();

    // StrategyContext 接口：收集策略信号。
    void emit_signal(const Signal& signal) override;
    // 供外部读取已收集的信号（示例用同步消费）。
    const std::vector<Signal>& signals() const { return signals_; }

private:
    ThreadPool pool_;
    std::vector<std::unique_ptr<StrategyBase>> strategies_;
    std::vector<Signal> signals_;
    std::mutex signal_mutex_;
};

} // namespace qf

