#pragma once

#include "qf/trading/trading_interface.hpp"

#include <mutex>

// 示例交易接口：直接回调成功（独立于 qf 框架）。
class DummyTradingInterface : public qf::ITradingInterface {
public:
    DummyTradingInterface();
    void start() override;
    void stop() override;
    void submit(const qf::OrderData& order, OrderCallback cb) override;
    void cancel(const std::string& order_id, OrderCallback cb) override;
    qf::AccountData get_account() const override;
    bool query_balance(double& available, double& frozen) const override;

private:
    qf::AccountData account_;
    mutable std::mutex mutex_;
};

