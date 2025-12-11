#pragma once

#include "qf/core/types.hpp"

namespace qf {

class SignalGenerator {
public:
    // 统一信号字段格式（大小写、默认值等）。
    Signal standardize(const Signal& raw) const;
};

} // namespace qf

