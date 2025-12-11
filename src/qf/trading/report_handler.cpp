#include "qf/trading/report_handler.hpp"

namespace qf {

void ReportHandler::on_order(const OrderData& order, OrderUpdateFn fn) const {
    // 透传订单回报给上层回调。
    fn(order);
}

void ReportHandler::on_trade(const TradeData& trade, TradeUpdateFn fn) const {
    // 透传成交回报给上层回调。
    fn(trade);
}

} // namespace qf

