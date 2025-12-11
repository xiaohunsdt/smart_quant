#include "sources/binance_data_source.hpp"
#include "qf/infrastructure/logger.hpp"
#include <fmt/core.h>
#include <optional>
#include <chrono>

namespace qf {

namespace {
std::optional<double> parse_price(const std::string& body) {
    // 期望格式: {"symbol":"BTCUSDT","price":"12345.67"}
    auto pos = body.find("\"price\":\"");
    if (pos == std::string::npos) return std::nullopt;
    pos += 9;
    auto end = body.find('"', pos);
    if (end == std::string::npos) return std::nullopt;
    try {
        return std::stod(body.substr(pos, end - pos));
    } catch (...) {
        return std::nullopt;
    }
}
} // namespace

BinanceDataSource::BinanceDataSource(std::string base_url,
                                     std::vector<std::string> symbols,
                                     std::size_t poll_interval_ms,
                                     std::size_t timeout_ms)
    : base_url_(std::move(base_url)),
      symbols_(std::move(symbols)),
      interval_ms_(poll_interval_ms),
      timeout_ms_(timeout_ms) {}

void BinanceDataSource::start(RawDataCallback cb) {
    if (symbols_.empty()) {
        Logger::instance().warn("BinanceDataSource no symbols configured");
        return;
    }
    running_.store(true);
    worker_ = std::thread([this, cb] {
        while (running_.load()) {
            for (const auto& sym : symbols_) {
                const auto url = fmt::format("{}/api/v3/ticker/price?symbol={}", base_url_, sym);
                auto resp = client_.get(url, timeout_ms_);
                if (!resp) {
                    Logger::instance().warn("BinanceDataSource request failed for " + sym);
                    continue;
                }
                auto price = parse_price(*resp);
                if (price) {
                    cb(fmt::format("{},{}", sym, *price));
                } else {
                    Logger::instance().warn("BinanceDataSource parse price failed for " + sym);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        }
    });
    Logger::instance().info("BinanceDataSource start");
}

void BinanceDataSource::stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
    Logger::instance().info("BinanceDataSource stop");
}

} // namespace qf

