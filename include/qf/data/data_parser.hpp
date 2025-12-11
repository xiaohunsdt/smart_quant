#pragma once

#include <string>
#include <functional>
#include "qf/core/types.hpp"

namespace qf {

class DataParser {
public:
    using ParsedCallback = std::function<void(const TickData&)>;
    // 将原始字符串解析为 TickData 并回调。
    void parse(const std::string& raw, ParsedCallback cb) const;
};

} // namespace qf

