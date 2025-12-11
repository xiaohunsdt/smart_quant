#include "qf/data/data_distributor.hpp"

namespace qf {

DataDistributor::DataDistributor(std::size_t workers) : pool_(workers) {}

DataDistributor::~DataDistributor() {
    stop();
}

void DataDistributor::subscribe(Subscriber sub) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.push_back(std::move(sub));
}

void DataDistributor::unsubscribe_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
}

void DataDistributor::publish(const TickData& tick) {
    std::vector<Subscriber> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = subscribers_;
    }
    // 使用线程池并行分发到所有订阅者。
    for (const auto& s : snapshot) {
        pool_.enqueue([s, tick] { s(tick); });
    }
}

void DataDistributor::stop() {
    pool_.stop();
}

} // namespace qf

