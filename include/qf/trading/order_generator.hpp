#pragma once

#include "qf/core/types.hpp"

namespace qf {

class OrderGenerator {
public:
    // 将策略信号转换为下单请求。
    OrderData from_signal(const Signal& signal) const;
};

} // namespace qf

