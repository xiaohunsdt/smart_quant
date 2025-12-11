#pragma once

#include <string>
#include <vector>

namespace qf {

struct LogConfig {
    enum class Level { Debug, Info, Warn, Error };
    Level level = Level::Info;      // 日志级别
    std::size_t file_size_mb = 5;   // 单个日志文件最大大小（MB）
    std::size_t max_files = 3;      // 保留文件数
};

struct RiskConfig {
    double position_limit = 5.0;                    // 单信号数量上限
    std::vector<std::string> compliance_whitelist;  // 合约白名单
    std::size_t workers = 2;                        // 风控引擎线程池大小
};

struct StrategyConfig {
    std::size_t workers = 2; // 策略线程池大小
};

struct DataConfig {
    std::string source_type = "dummy";           // 数据源类型：dummy/binance
    std::size_t distributor_workers = 4;          // 数据分发器线程池大小
};

struct BinanceConfig {
    bool enable = false;                         // 是否启用 Binance 数据源
    std::string base_url = "https://api.binance.com"; // Binance API 基地址
    std::vector<std::string> symbols;            // 订阅的交易对
    std::size_t poll_interval_ms = 1000;         // 轮询间隔（ms，REST）
    std::size_t timeout_ms = 2000;               // HTTP 超时（ms）
};

struct AppConfig {
    LogConfig log;
    RiskConfig risk;
    StrategyConfig strategy;
    DataConfig data;
    BinanceConfig binance;
};

// 统一的 YAML 配置管理器。
class ConfigManager {
public:
    // 加载 YAML 文件。
    bool load(const std::string& path);
    // 获取解析后的配置。
    const AppConfig& get() const { return config_; }

private:
    static LogConfig::Level parse_log_level(const std::string& v);

    AppConfig config_;
};

} // namespace qf

