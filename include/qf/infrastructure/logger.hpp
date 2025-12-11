#pragma once

#include <string>

#include <spdlog/spdlog.h>

namespace qf {

class Logger {
public:
    enum class Level { Debug, Info, Warn, Error };

    // 获取全局 Logger 单例。
    static Logger& instance();

    // 按级别输出日志（基于 spdlog）- 保留以保持向后兼容。
    void log(Level level, const std::string& message);
    
    // 模板方法：支持 spdlog 格式化（利用 spdlog 内部集成的 fmt）- 保留以保持向后兼容。
    template<typename... Args>
    void log(Level level, const std::string& format, Args&&... args) {
        switch (level) {
        case Level::Debug: spdlog::debug(format, std::forward<Args>(args)...); break;
        case Level::Info: spdlog::info(format, std::forward<Args>(args)...); break;
        case Level::Warn: spdlog::warn(format, std::forward<Args>(args)...); break;
        case Level::Error: spdlog::error(format, std::forward<Args>(args)...); break;
        }
    }
    
    // 便捷方法：直接按级别输出日志（支持格式化）。
    void debug(const std::string& message) { spdlog::debug("{}", message); }
    void info(const std::string& message) { spdlog::info("{}", message); }
    void warn(const std::string& message) { spdlog::warn("{}", message); }
    void error(const std::string& message) { spdlog::error("{}", message); }
    
    // 模板方法：支持 spdlog 格式化（利用 spdlog 内部集成的 fmt）。
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        spdlog::debug(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        spdlog::info(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        spdlog::warn(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        spdlog::error(format, std::forward<Args>(args)...);
    }
    
    // 设置最小输出级别。
    void set_level(Level level);

private:
    Logger() = default;
};

} // namespace qf

