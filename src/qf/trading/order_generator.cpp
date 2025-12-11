#include "qf/trading/order_generator.hpp"

namespace qf {

OrderData OrderGenerator::from_signal(const Signal& signal) const {
    OrderData order;
    order.order_id = signal.strategy_id + "_" + signal.symbol + "_" + std::to_string(signal.ts.time_since_epoch().count());
    order.strategy_id = signal.strategy_id;
    order.symbol = signal.symbol;
    order.price = signal.price;
    order.volume = signal.volume;
    order.side = signal.side;
    order.type = signal.order_type;
    order.status = OrderStatus::Pending;
    order.ts = signal.ts;
    // 生成的订单可直接交给交易接口提交。
    return order;
}

} // namespace qf

