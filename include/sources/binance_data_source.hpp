#pragma once

#include "qf/data/data_source.hpp"
#include "qf/common/http_client.hpp"
#include <atomic>
#include <thread>

namespace qf {

// Binance 数据源（REST 轮询示例）。
class BinanceDataSource : public IDataSource {
public:
    BinanceDataSource(std::string base_url,
                      std::vector<std::string> symbols,
                      std::size_t poll_interval_ms,
                      std::size_t timeout_ms);
    void start(RawDataCallback cb) override;
    void stop() override;
    std::string name() const override { return "binance"; }

private:
    std::string base_url_;
    std::vector<std::string> symbols_;
    std::size_t interval_ms_;
    std::size_t timeout_ms_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    HttpClient client_;
};

} // namespace qf

