#pragma once

#include <functional>
#include <string>
#include <memory>
#include "qf/core/types.hpp"

namespace qf {

class IDataSource {
public:
    using RawDataCallback = std::function<void(const std::string&)>;
    virtual ~IDataSource() = default;
    // 启动数据源，原始数据通过回调推送。
    virtual void start(RawDataCallback cb) = 0;
    // 停止数据源。
    virtual void stop() = 0;
    // 数据源名称用于监控/日志。
    virtual std::string name() const = 0;
};

} // namespace qf

