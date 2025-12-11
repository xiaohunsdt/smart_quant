#pragma once

#include "qf/core/types.hpp"
#include <functional>

namespace qf {

class ReportHandler {
public:
    using OrderUpdateFn = std::function<void(const OrderData&)>;
    using TradeUpdateFn = std::function<void(const TradeData&)>;

    // 处理订单状态回报并回调上层。
    void on_order(const OrderData& order, OrderUpdateFn fn) const;
    // 处理成交回报并回调上层。
    void on_trade(const TradeData& trade, TradeUpdateFn fn) const;
};

} // namespace qf

