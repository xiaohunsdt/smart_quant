#include "qf/risk/risk_engine.hpp"
#include "qf/trading/account_manager.hpp"
#include "qf/infrastructure/logger.hpp"
#include <future>

namespace qf {

PositionLimitRule::PositionLimitRule(double max_volume) : max_volume_(max_volume) {}

bool PositionLimitRule::validate(const Signal& signal) {
    // 简单判断信号数量是否超限。
    return signal.volume <= max_volume_;
}

RiskEngine::RiskEngine(std::size_t workers) : pool_(workers) {}

RiskEngine::~RiskEngine() {
    stop();
}

void RiskEngine::add_rule(std::unique_ptr<RiskRule> rule) {
    rules_.push_back(std::move(rule));
}

bool RiskEngine::check(const Signal& signal) const {
    // 并行执行所有规则校验，使用 ThreadPool。
    if (rules_.empty()) {
        // 如果没有规则，直接进行资产校验。
        if (account_mgr_) {
            double required_amount = signal.price * signal.volume;
            if (!account_mgr_->has_sufficient_balance(required_amount)) {
                Logger::instance().warn(
                    "风控检查失败: 资金不足。所需金额: {:.2f}, 可用资金: {:.2f}", 
                    required_amount, account_mgr_->get_available_balance());
                return false;
            }
        }
        return true;
    }
    
    std::vector<std::promise<bool>> promises(rules_.size());
    std::vector<std::future<bool>> futures;
    std::vector<std::string> rule_names;
    
    for (auto& p : promises) {
        futures.push_back(p.get_future());
    }
    
    // 保存规则名称，用于日志。
    for (const auto& r : rules_) {
        rule_names.push_back(r->name());
    }
    
    // 将规则校验任务投递到线程池（使用值捕获确保线程安全）。
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        auto* rule = rules_[i].get();
        auto& promise = promises[i];
        Signal signal_copy = signal;  // 复制信号以确保线程安全
        pool_.enqueue([rule, signal_copy, &promise] {
            promise.set_value(rule->validate(signal_copy));
        });
    }
    
    // 等待所有规则执行完成，所有规则均通过才认为可下单。
    for (std::size_t i = 0; i < futures.size(); ++i) {
        if (!futures[i].get()) {
            Logger::instance().warn(
                "风控规则阻止信号: {} 策略={} 合约={} 数量={}", 
                rule_names[i], signal.strategy_id, signal.symbol, signal.volume);
            return false;
        }
    }
    
    // 如果有账户管理器，进行资产校验。
    if (account_mgr_) {
        double required_amount = signal.price * signal.volume;
        if (!account_mgr_->has_sufficient_balance(required_amount)) {
            Logger::instance().warn(
                "风控检查失败: 资金不足。所需金额: {:.2f}, 可用资金: {:.2f}", 
                required_amount, account_mgr_->get_available_balance());
            return false;
        }
    }
    
    return true;
}

void RiskEngine::set_account_manager(AccountManager* account_mgr) {
    account_mgr_ = account_mgr;
}

bool RiskEngine::check_balance(const OrderData& order) const {
    if (!account_mgr_) {
        Logger::instance().warn("风控引擎: 账户管理器未设置，跳过资金检查");
        return true;
    }
    
    double required_amount = order.price * order.volume;
    if (!account_mgr_->has_sufficient_balance(required_amount)) {
        Logger::instance().warn(
            "订单风控检查失败 {}: 资金不足。所需金额: {:.2f}, 可用资金: {:.2f}", 
            order.order_id, required_amount, account_mgr_->get_available_balance());
        return false;
    }
    return true;
}

void RiskEngine::stop() {
    pool_.stop();
}

} // namespace qf

