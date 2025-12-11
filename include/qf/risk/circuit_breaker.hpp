#pragma once

#include <atomic>

namespace qf {

class CircuitBreaker {
public:
    // 触发熔断。
    void trip();
    // 恢复熔断。
    void reset();
    // 当前是否已熔断。
    bool tripped() const;

private:
    std::atomic<bool> tripped_{false};
};

} // namespace qf

