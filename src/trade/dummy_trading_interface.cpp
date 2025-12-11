#include "trade/dummy_trading_interface.hpp"
#include "qf/infrastructure/logger.hpp"
#include <mutex>

DummyTradingInterface::DummyTradingInterface() {
    account_.account_id = "dummy_account";
    account_.total_asset = 1000000.0;      // 初始总资产 100万
    account_.available_balance = 1000000.0; // 初始可用资金 100万
    account_.frozen_balance = 0.0;
    account_.position_value = 0.0;
    account_.unrealized_pnl = 0.0;
    account_.realized_pnl = 0.0;
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

void DummyTradingInterface::start() {
    qf::Logger::instance().info("交易接口已启动");
}

void DummyTradingInterface::stop() {
    qf::Logger::instance().info("交易接口已停止");
}

void DummyTradingInterface::submit(const qf::OrderData& order, OrderCallback cb) {
    qf::Logger::instance().info("提交订单: " + order.order_id);
    qf::OrderData copy = order;
    copy.status = qf::OrderStatus::Submitted;
    // 直接同步回调，实际场景应等待交易所回报。
    cb(copy);
}

void DummyTradingInterface::cancel(const std::string& order_id, OrderCallback cb) {
    qf::Logger::instance().info("取消订单: " + order_id);
    qf::OrderData ord;
    ord.order_id = order_id;
    ord.status = qf::OrderStatus::Cancelled;
    cb(ord);
}

qf::AccountData DummyTradingInterface::get_account() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_;
}

bool DummyTradingInterface::query_balance(double& available, double& frozen) const {
    std::lock_guard<std::mutex> lock(mutex_);
    available = account_.available_balance;
    frozen = account_.frozen_balance;
    return true;
}

