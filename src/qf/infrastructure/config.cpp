#include "qf/infrastructure/config.hpp"
#include <yaml-cpp/yaml.h>

namespace qf {

namespace {
std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(::tolower(c));
    return s;
}
}

LogConfig::Level ConfigManager::parse_log_level(const std::string& v) {
    const auto lower = to_lower(v);
    if (lower == "debug") return LogConfig::Level::Debug;
    if (lower == "warn") return LogConfig::Level::Warn;
    if (lower == "error") return LogConfig::Level::Error;
    return LogConfig::Level::Info;
}

bool ConfigManager::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const std::exception&) {
        return false;
    }

    // 日志配置
    if (root["log"]) {
        auto log = root["log"];
        if (log["level"]) config_.log.level = parse_log_level(log["level"].as<std::string>());
        if (log["file_size_mb"]) config_.log.file_size_mb = log["file_size_mb"].as<std::size_t>();
        if (log["max_files"]) config_.log.max_files = log["max_files"].as<std::size_t>();
    }

    // 风控配置
    if (root["risk"]) {
        auto risk = root["risk"];
        if (risk["position_limit"]) config_.risk.position_limit = risk["position_limit"].as<double>();
        if (risk["compliance_whitelist"]) {
            config_.risk.compliance_whitelist = risk["compliance_whitelist"].as<std::vector<std::string>>();
        }
        if (risk["workers"]) config_.risk.workers = risk["workers"].as<std::size_t>();
    }

    // 策略配置
    if (root["strategy"]) {
        auto strategy = root["strategy"];
        if (strategy["workers"]) config_.strategy.workers = strategy["workers"].as<std::size_t>();
    }

    // 数据源配置
    if (root["data"]) {
        auto data = root["data"];
        if (data["source_type"]) config_.data.source_type = data["source_type"].as<std::string>();
        if (data["distributor_workers"]) config_.data.distributor_workers = data["distributor_workers"].as<std::size_t>();
    }

    // Binance 配置
    if (root["binance"]) {
        auto binance = root["binance"];
        if (binance["enable"]) config_.binance.enable = binance["enable"].as<bool>();
        if (binance["base_url"]) config_.binance.base_url = binance["base_url"].as<std::string>();
        if (binance["symbols"]) config_.binance.symbols = binance["symbols"].as<std::vector<std::string>>();
        if (binance["poll_interval_ms"]) config_.binance.poll_interval_ms = binance["poll_interval_ms"].as<std::size_t>();
        if (binance["timeout_ms"]) config_.binance.timeout_ms = binance["timeout_ms"].as<std::size_t>();
    }

    return true;
}

} // namespace qf

