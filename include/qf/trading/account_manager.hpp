#pragma once

#include "qf/core/types.hpp"
#include <mutex>
#include <vector>

namespace qf {

// 前向声明。
class OrderManager;
class PositionManager;
class ITradingInterface;

// 账户管理器：管理账户信息，包括总资产、订单、持仓等。
class AccountManager {
public:
    AccountManager();
    
    // 账户基本信息管理。
    void set_account_id(const std::string& account_id);
    std::string get_account_id() const;
    
    // 更新账户资金信息。
    void update_balance(double available, double frozen = 0.0);
    void update_total_asset(double total_asset);
    
    // 获取账户信息。
    AccountData get_account() const;
    
    // 计算并更新持仓市值和浮动盈亏（需要最新行情价格）。
    void update_position_value(const std::string& symbol, double market_price);
    void update_unrealized_pnl();
    
    // 设置关联的管理器。
    void set_order_manager(OrderManager* order_mgr);
    void set_position_manager(PositionManager* position_mgr);
    void set_trading_interface(ITradingInterface* trading);
    
    // 从交易接口同步账户信息。
    bool sync_from_trading_interface();
    
    // 获取当前所有订单（从 OrderManager 获取）。
    std::vector<OrderData> get_all_orders() const;
    
    // 获取活跃订单（未完成订单）。
    std::vector<OrderData> get_active_orders() const;
    
    // 获取账户总资产（包含持仓市值和浮动盈亏）。
    double get_total_asset() const;
    
    // 获取可用资金。
    double get_available_balance() const;
    
    // 获取持仓市值。
    double get_position_value() const;
    
    // 获取浮动盈亏。
    double get_unrealized_pnl() const;
    
    // 检查是否有足够资金下单。
    bool has_sufficient_balance(double required_amount) const;

private:
    mutable std::mutex mutex_;
    AccountData account_;
    OrderManager* order_mgr_{nullptr};
    PositionManager* position_mgr_{nullptr};
    ITradingInterface* trading_{nullptr};
    
    // 计算持仓市值（需要 PositionManager 和最新行情）。
    double calculate_position_value() const;
};

} // namespace qf

