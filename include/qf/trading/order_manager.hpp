#pragma once

#include <unordered_map>
#include <mutex>
#include "qf/core/types.hpp"

namespace qf {

class OrderManager {
public:
    // 新增或更新订单信息。
    void upsert(const OrderData& order);
    // 按 ID 查询订单。
    OrderData get(const std::string& order_id) const;
    // 获取全部订单快照。
    std::vector<OrderData> list() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, OrderData> orders_;
};

} // namespace qf

