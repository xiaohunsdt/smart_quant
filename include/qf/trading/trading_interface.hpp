#pragma once

#include <string>
#include <functional>
#include "qf/core/types.hpp"

namespace qf {

class ITradingInterface {
public:
    using OrderCallback = std::function<void(const OrderData&)>;
    using TradeCallback = std::function<void(const TradeData&)>;
    virtual ~ITradingInterface() = default;
    // 启动并保持与交易系统连接。
    virtual void start() = 0;
    virtual void stop() = 0;
    // 提交订单 / 撤单，结果通过回调返回。
    virtual void submit(const OrderData& order, OrderCallback cb) = 0;
    virtual void cancel(const std::string& order_id, OrderCallback cb) = 0;
    
    // 获取账户信息（从交易接口获取真实数据）。
    virtual AccountData get_account() const = 0;
    
    // 查询账户资金（可用资金、冻结资金等）。
    virtual bool query_balance(double& available, double& frozen) const = 0;
};

} // namespace qf

