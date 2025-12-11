#include "sources/dummy_data_source.hpp"
#include "qf/infrastructure/logger.hpp"
#include <thread>
#include <chrono>

namespace qf {

void DummyDataSource::start(RawDataCallback cb) {
    running_.store(true);
    Logger::instance().info("模拟数据源已启动");
    // 简单同步推送少量伪行情。
    for (int i = 0; running_.load() && i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cb("dummy_symbol," + std::to_string(100 + i));
    }
}

void DummyDataSource::stop() {
    running_.store(false);
    Logger::instance().info("模拟数据源已停止");
}

} // namespace qf

