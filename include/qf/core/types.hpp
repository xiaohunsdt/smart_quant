#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace qf {

// Framework-wide microsecond timestamp.
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

// 订单方向枚举。
enum class OrderSide {
    Buy,
    Sell
};

// 订单类型枚举。
enum class OrderType {
    Limit,
    Market
};

// 订单状态枚举。
enum class OrderStatus {
    Pending,
    Submitted,
    Partial,
    Filled,
    Cancelled,
    Rejected
};

// 标准化 Tick 行情数据，适配各类数据源。
struct TickData {
    std::string symbol;
    double last_price{0.0};
    double bid_price{0.0};
    double ask_price{0.0};
    double bid_volume{0.0};
    double ask_volume{0.0};
    Timestamp ts{};
};

// 订单全生命周期的标准数据结构。
struct OrderData {
    std::string order_id;
    std::string strategy_id;
    std::string symbol;
    double price{0.0};
    double volume{0.0};
    OrderSide side{OrderSide::Buy};
    OrderType type{OrderType::Limit};
    OrderStatus status{OrderStatus::Pending};
    Timestamp ts{};
};

// 成交回报数据结构。
struct TradeData {
    std::string order_id;
    std::string trade_id;
    std::string symbol;
    double price{0.0};
    double volume{0.0};
    Timestamp ts{};
};

// 持仓快照数据结构。
struct PositionData {
    std::string symbol;
    double long_qty{0.0};
    double short_qty{0.0};
    double available{0.0};
    double avg_price{0.0};
    double unrealized_pnl{0.0};
};

// 策略输出的标准化交易信号。
struct Signal {
    std::string strategy_id;
    std::string symbol;
    OrderSide side{OrderSide::Buy};
    double price{0.0};
    double volume{0.0};
    OrderType order_type{OrderType::Limit};
    Timestamp ts{};
};

// 账户信息数据结构。
struct AccountData {
    std::string account_id;              // 账户ID
    double total_asset{0.0};             // 总资产（权益）
    double available_balance{0.0};       // 可用资金
    double frozen_balance{0.0};          // 冻结资金
    double position_value{0.0};          // 持仓市值
    double unrealized_pnl{0.0};          // 浮动盈亏
    double realized_pnl{0.0};            // 已实现盈亏
    Timestamp update_time{};             // 更新时间
};

} // namespace qf

