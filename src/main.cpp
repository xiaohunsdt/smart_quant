#include "qf/infrastructure/logger.hpp"
#include "qf/infrastructure/config.hpp"
#include "qf/data/data_source.hpp"
#include "sources/dummy_data_source.hpp"
#include "sources/binance_data_source.hpp"
#include "qf/data/data_parser.hpp"
#include "qf/data/data_distributor.hpp"
#include "qf/data/market_cache.hpp"
#include "qf/strategy/strategy_scheduler.hpp"
#include "qf/strategy/signal_generator.hpp"
#include "qf/trading/order_generator.hpp"
#include "qf/trading/order_manager.hpp"
#include "qf/trading/position_manager.hpp"
#include "qf/trading/account_manager.hpp"
#include "qf/trading/trading_interface.hpp"
#include "qf/trading/report_handler.hpp"
#include "trade/dummy_trading_interface.hpp"
#include "qf/risk/risk_engine.hpp"
#include "qf/risk/compliance_checker.hpp"
#include "strategies/sample_strategy.hpp"
#include <iostream>

using namespace qf;

int main() {
    // 加载 YAML 配置。
    ConfigManager cfg_mgr;
    if (!cfg_mgr.load("config/config.yaml")) {
        std::cerr << "Failed to load config/config.yaml" << std::endl;
        return 1;
    }
    const auto& cfg = cfg_mgr.get();

    // 设置日志级别。
    switch (cfg.log.level) {
    case LogConfig::Level::Debug: Logger::instance().set_level(Logger::Level::Debug); break;
    case LogConfig::Level::Warn: Logger::instance().set_level(Logger::Level::Warn); break;
    case LogConfig::Level::Error: Logger::instance().set_level(Logger::Level::Error); break;
    case LogConfig::Level::Info:
    default: Logger::instance().set_level(Logger::Level::Info); break;
    }

    std::unique_ptr<IDataSource> source;
    if (cfg.data.source_type == "binance" || cfg.binance.enable) {
        source = std::make_unique<BinanceDataSource>(
            cfg.binance.base_url,
            cfg.binance.symbols,
            cfg.binance.poll_interval_ms,
            cfg.binance.timeout_ms);
    } else {
        source = std::make_unique<DummyDataSource>();
    }

    // 策略初始化
    StrategyScheduler scheduler(cfg.strategy.workers);
    scheduler.add_strategy(std::make_unique<SampleStrategy>("sma-demo"));

    // 风控初始化
    RiskEngine risk(cfg.risk.workers);
    risk.add_rule(std::make_unique<PositionLimitRule>(cfg.risk.position_limit));
    ComplianceChecker compliance;
    compliance.set_symbol_whitelist(cfg.risk.compliance_whitelist);

    // 交易初始化
    DummyTradingInterface trading;
    trading.start();

    OrderManager order_mgr;
    PositionManager pos_mgr;

    
    // 从交易接口同步账户信息。
    AccountManager account_mgr;
    account_mgr.set_order_manager(&order_mgr);
    account_mgr.set_position_manager(&pos_mgr);
    account_mgr.set_trading_interface(&trading);
    
    if (!account_mgr.sync_from_trading_interface()) {
        Logger::instance().warn("Failed to sync account from trading interface, using default");
        account_mgr.set_account_id("demo_account");
        account_mgr.update_balance(1000000.0, 0.0); // 默认初始资金 100万
    }
    
    // 关联账户管理器到风控引擎。
    risk.set_account_manager(&account_mgr);
    
    ReportHandler reports;

    // 启动数据源，原始字符串 -> Tick 解析 -> 缓存 + 分发 + 策略执行。
    MarketCache cache;
    DataParser parser;
    DataDistributor distributor(cfg.data.distributor_workers);

    source->start([&](const std::string& raw) {
        parser.parse(raw, [&](const TickData& tick) {
            cache.put(tick);
            distributor.publish(tick);
            scheduler.on_tick(tick);
        });
    });

    // 同步消费策略信号：标准化 -> 合规/风控 -> 生成订单 -> 提交。
    SignalGenerator signal_gen;
    OrderGenerator order_gen;
    for (const auto& sig : scheduler.signals()) {
        auto normalized = signal_gen.standardize(sig);
        if (!compliance.allowed(normalized)) continue;
        Logger::instance().info("Compliance allowed signal: {} strategy={} symbol={} volume={}", normalized.strategy_id, normalized.symbol, normalized.volume);

        if (!risk.check(normalized)) continue;
        Logger::instance().info("Risk check passed signal: {} strategy={} symbol={} volume={}", normalized.strategy_id, normalized.symbol, normalized.volume);
        
        auto order = order_gen.from_signal(normalized);
        
        // 交易前再次进行资产校验。
        if (!risk.check_balance(order)) {
            Logger::instance().warn( 
                "Order rejected due to insufficient balance: order_id={}, required={:.2f}", 
                order.order_id, order.price * order.volume);
            continue;
        }
        
        trading.submit(order, [&](const OrderData& od) {
            order_mgr.upsert(od);
            // 订单提交后，可以更新账户冻结资金（实际应从交易接口获取）。
        });
    }

    while (true) {}

    // 停止所有策略和交易
    scheduler.stop_all();
    distributor.stop();  // 停止数据分发器线程池
    risk.stop();  // 停止风控引擎线程池
    trading.stop();

    // 打印结果，用于验证流程跑通。
    Logger::instance().info("Total orders: {}", order_mgr.list().size());
    
    // 打印账户信息。
    auto account = account_mgr.get_account();
    Logger::instance().info( 
        "Account ID: {}, Total Asset: {:.2f}, Available: {:.2f}, Active Orders: {}", 
        account.account_id, 
        account_mgr.get_total_asset(), 
        account_mgr.get_available_balance(),
        account_mgr.get_active_orders().size());
    
    return 0;
}

