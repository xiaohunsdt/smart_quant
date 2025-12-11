#include "qf/infrastructure/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace qf {

std::once_flag init_flag;

Logger& Logger::instance() {
    static Logger inst;
    // 初始化彩色控制台 logger 与输出格式。
    std::call_once(init_flag, [] {
#ifdef NDEBUG
        // Release：控制台 + 滚动文件输出。
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/framework.log", 5 * 1024 * 1024, 3);
        console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%l]%$ %v");
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%l] %v");
        auto logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
        spdlog::set_default_logger(logger);
#else
        // Debug：仅控制台彩色输出。
        auto logger = spdlog::stdout_color_mt("console");
        logger->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%l]%$ %v");
        spdlog::set_default_logger(logger);
#endif
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);

        spdlog::info("Logger initialized! Logger level: {}", std::to_string(spdlog::level::info));
        return true;
    });
    
    return inst;
}

void Logger::set_level(Level level) {
    // 配置 spdlog 全局级别。
    switch (level) {
    case Level::Debug: spdlog::set_level(spdlog::level::debug); break;
    case Level::Info: spdlog::set_level(spdlog::level::info); break;
    case Level::Warn: spdlog::set_level(spdlog::level::warn); break;
    case Level::Error: spdlog::set_level(spdlog::level::err); break;
    }
}

void Logger::log(Level level, const std::string& message) {
    // 映射到 spdlog 级别。
    switch (level) {
    case Level::Debug: spdlog::debug("{}", message); break;
    case Level::Info: spdlog::info("{}", message); break;
    case Level::Warn: spdlog::warn("{}", message); break;
    case Level::Error: spdlog::error("{}", message); break;
    }
}

} // namespace qf

