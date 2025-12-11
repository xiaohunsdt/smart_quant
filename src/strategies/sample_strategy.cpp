#include "strategies/sample_strategy.hpp"

namespace qf {

SampleStrategy::SampleStrategy(std::string id) : id_(std::move(id)) {}

std::string SampleStrategy::id() const { return id_; }

void SampleStrategy::on_init() {
    // 示例策略初始化占位。
}

void SampleStrategy::on_tick(const TickData& tick) {
    // 收到 Tick 后直接发出买入信号。
    Signal sig;
    sig.strategy_id = id_;
    sig.symbol = tick.symbol;
    sig.side = OrderSide::Buy;
    sig.price = tick.last_price;
    sig.volume = 1;
    sig.ts = tick.ts;
    if (ctx_) ctx_->emit_signal(sig);
}

void SampleStrategy::on_stop() {
    // 停止钩子占位。
}

} // namespace qf

