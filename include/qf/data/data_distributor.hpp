#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include "qf/core/types.hpp"
#include "qf/infrastructure/thread_pool.hpp"

namespace qf {

class DataDistributor {
public:
    using Subscriber = std::function<void(const TickData&)>;

    // 创建数据分发器，使用线程池并行分发。
    explicit DataDistributor(std::size_t workers = 4);
    ~DataDistributor();

    // 注册订阅者（策略/风控/交易等）。
    void subscribe(Subscriber sub);
    // 清空所有订阅。
    void unsubscribe_all();
    // 发布行情到所有订阅者（并行分发）。
    void publish(const TickData& tick);
    // 停止线程池。
    void stop();

private:
    std::vector<Subscriber> subscribers_;
    std::mutex mutex_;
    ThreadPool pool_;
};

} // namespace qf

