#include "qf/strategy/strategy_scheduler.hpp"
#include "qf/infrastructure/logger.hpp"

namespace qf {

StrategyScheduler::StrategyScheduler(std::size_t workers) : pool_(workers) {}

void StrategyScheduler::add_strategy(std::unique_ptr<StrategyBase> strategy) {
    strategy->set_context(this);
    strategy->on_init();
    strategies_.push_back(std::move(strategy));
}

void StrategyScheduler::on_tick(const TickData& tick) {
    // 将行情任务异步投递给各策略。
    for (auto& s : strategies_) {
        pool_.enqueue([&s, tick] { s->on_tick(tick); });
    }
}

void StrategyScheduler::stop_all() {
    for (auto& s : strategies_) {
        s->on_stop();
    }
    pool_.stop();
}

void StrategyScheduler::emit_signal(const Signal& signal) {
    std::lock_guard<std::mutex> lock(signal_mutex_);
    signals_.push_back(signal);
    // 记录信号来源，方便调试。
    Logger::instance().info("策略信号已发出，来源: " + signal.strategy_id);
}

} // namespace qf

