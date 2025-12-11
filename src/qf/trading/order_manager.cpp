#include "qf/trading/order_manager.hpp"

namespace qf {

void OrderManager::upsert(const OrderData& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 以订单 ID 作为唯一键存储。
    orders_[order.order_id] = order;
}

OrderData OrderManager::get(const std::string& order_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        return it->second;
    }
    return {};
}

std::vector<OrderData> OrderManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<OrderData> out;
    out.reserve(orders_.size());
    for (auto& kv : orders_) {
        out.push_back(kv.second);
    }
    return out;
}

} // namespace qf

