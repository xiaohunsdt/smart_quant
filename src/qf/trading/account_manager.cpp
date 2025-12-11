#include "qf/trading/account_manager.hpp"
#include "qf/trading/order_manager.hpp"
#include "qf/trading/position_manager.hpp"
#include "qf/trading/trading_interface.hpp"
#include "qf/infrastructure/logger.hpp"
#include <algorithm>
#include <iterator>
#include <mutex>
#include <string>

namespace qf {

AccountManager::AccountManager() {
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

void AccountManager::set_account_id(const std::string& account_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    account_.account_id = account_id;
}

std::string AccountManager::get_account_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_.account_id;
}

void AccountManager::update_balance(double available, double frozen) {
    std::lock_guard<std::mutex> lock(mutex_);
    account_.available_balance = available;
    account_.frozen_balance = frozen;
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

void AccountManager::update_total_asset(double total_asset) {
    std::lock_guard<std::mutex> lock(mutex_);
    account_.total_asset = total_asset;
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

AccountData AccountManager::get_account() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_;
}

void AccountManager::update_position_value(const std::string& symbol, double market_price) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 如果有 PositionManager，可以基于持仓和最新价格计算持仓市值。
    // 这里简化处理，实际应该从 PositionManager 获取持仓。
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

void AccountManager::update_unrealized_pnl() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 基于持仓和最新价格计算浮动盈亏。
    // 实际实现需要从 PositionManager 获取持仓信息。
    account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
}

void AccountManager::set_order_manager(OrderManager* order_mgr) {
    std::lock_guard<std::mutex> lock(mutex_);
    order_mgr_ = order_mgr;
}

void AccountManager::set_position_manager(PositionManager* position_mgr) {
    std::lock_guard<std::mutex> lock(mutex_);
    position_mgr_ = position_mgr;
}

void AccountManager::set_trading_interface(ITradingInterface* trading) {
    std::lock_guard<std::mutex> lock(mutex_);
    trading_ = trading;
}

bool AccountManager::sync_from_trading_interface() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!trading_) {
        Logger::instance().warn("AccountManager: trading interface not set");
        return false;
    }
    
    try {
        account_ = trading_->get_account();
        account_.update_time = std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now());
        Logger::instance().info( 
            "Account synced: available={:.2f}, frozen={:.2f}, total={:.2f}", 
            account_.available_balance, account_.frozen_balance, account_.total_asset);
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error( 
            "AccountManager: failed to sync from trading interface: {}", std::string(e.what()));
        return false;
    }
}

std::vector<OrderData> AccountManager::get_all_orders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (order_mgr_) {
        return order_mgr_->list();
    }
    return {};
}

std::vector<OrderData> AccountManager::get_active_orders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!order_mgr_) return {};
    
    auto all_orders = order_mgr_->list();
    std::vector<OrderData> active;
    active.reserve(all_orders.size());
    for (const auto& order : all_orders) {
        if (order.status == OrderStatus::Pending ||
            order.status == OrderStatus::Submitted ||
            order.status == OrderStatus::Partial) {
            active.push_back(order);
        }
    }
    return active;
}

double AccountManager::get_total_asset() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // 总资产 = 可用资金 + 冻结资金 + 持仓市值 + 浮动盈亏
    return account_.available_balance + account_.frozen_balance + 
           account_.position_value + account_.unrealized_pnl;
}

double AccountManager::get_available_balance() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_.available_balance;
}

double AccountManager::get_position_value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_.position_value;
}

double AccountManager::get_unrealized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_.unrealized_pnl;
}

bool AccountManager::has_sufficient_balance(double required_amount) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_.available_balance >= required_amount;
}

double AccountManager::calculate_position_value() const {
    // 计算持仓市值，需要 PositionManager 和最新行情。
    if (!position_mgr_) return 0.0;
    
    double total_value = 0.0;
    auto positions = position_mgr_->list();
    for (const auto& pos : positions) {
        // 持仓市值 = 持仓数量 * 最新价格
        // 这里简化处理，实际需要从 MarketCache 获取最新价格
        // total_value += (pos.long_qty - pos.short_qty) * market_price;
    }
    return total_value;
}

} // namespace qf

