#pragma once

#include <string>
#include <functional>
#include "qf/core/types.hpp"

namespace qf {

class HistoricalLoader {
public:
    using ReplayCallback = std::function<void(const TickData&)>;
    // 从文件加载历史行情并按顺序回放。
    void load_and_replay(const std::string& path, ReplayCallback cb) const;
};

} // namespace qf

