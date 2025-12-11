#include "qf/strategy/signal_generator.hpp"

namespace qf {

Signal SignalGenerator::standardize(const Signal& raw) const {
    Signal out = raw;
    // 确保订单类型有默认值。
    // 其他字段可在此补全/归一化。
    return out;
}

} // namespace qf

