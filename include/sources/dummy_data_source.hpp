#pragma once

#include "qf/data/data_source.hpp"
#include <atomic>

namespace qf {

// 示例数据源：生成简单的伪行情。
class DummyDataSource : public IDataSource {
public:
    void start(RawDataCallback cb) override;
    void stop() override;
    std::string name() const override { return "dummy"; }

private:
    std::atomic<bool> running_{false};
};

} // namespace qf

