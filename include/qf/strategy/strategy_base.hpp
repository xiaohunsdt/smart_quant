#pragma once

#include <string>
#include "qf/core/types.hpp"

namespace qf {

class StrategyContext {
public:
    virtual ~StrategyContext() = default;
    // 策略通过上下文把信号回传给调度器。
    virtual void emit_signal(const Signal& signal) = 0;
};

class StrategyBase {
public:
    virtual ~StrategyBase() = default;
    // 策略唯一标识。
    virtual std::string id() const = 0;
    // 初始化钩子。
    virtual void on_init() = 0;
    // 行情回调。
    virtual void on_tick(const TickData& tick) = 0;
    // 停止钩子。
    virtual void on_stop() = 0;
    // 由调度器注入上下文。
    void set_context(StrategyContext* ctx) { ctx_ = ctx; }

protected:
    StrategyContext* ctx_{nullptr};
};

} // namespace qf

