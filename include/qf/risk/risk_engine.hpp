#pragma once

#include <vector>
#include <memory>
#include <future>
#include "qf/core/types.hpp"
#include "qf/infrastructure/thread_pool.hpp"

namespace qf {

class RiskRule {
public:
    virtual ~RiskRule() = default;
    // 校验信号是否通过规则。
    virtual bool validate(const Signal& signal) = 0;
    // 规则名称用于日志/监控。
    virtual std::string name() const = 0;
};

class PositionLimitRule : public RiskRule {
public:
    explicit PositionLimitRule(double max_volume);
    // 限制信号数量不得超过阈值。
    bool validate(const Signal& signal) override;
    std::string name() const override { return "position_limit"; }

private:
    double max_volume_;
};

// 前向声明。
class AccountManager;

class RiskEngine {
public:
    // 创建风控引擎，使用线程池并行执行规则。
    explicit RiskEngine(std::size_t workers = 2);
    ~RiskEngine();

    // 注册风控规则。
    void add_rule(std::unique_ptr<RiskRule> rule);
    // 对单条信号执行所有规则校验（并行执行）。
    bool check(const Signal& signal) const;
    // 设置账户管理器，用于资产校验。
    void set_account_manager(AccountManager* account_mgr);
    // 对订单进行资产校验（检查是否有足够资金）。
    bool check_balance(const OrderData& order) const;
    // 停止线程池。
    void stop();

private:
    std::vector<std::unique_ptr<RiskRule>> rules_;
    AccountManager* account_mgr_{nullptr};
    mutable ThreadPool pool_;
};

} // namespace qf

