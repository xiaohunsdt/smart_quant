#include "qf/data/data_parser.hpp"
#include "qf/infrastructure/logger.hpp"

namespace qf {

void DataParser::parse(const std::string& raw, ParsedCallback cb) const {
    // Very small placeholder parser: raw format "symbol,price"
    Logger::instance().debug("DataParser parse raw={}", raw);
    
    auto pos = raw.find(',');
    if (pos == std::string::npos) return;
    TickData tick;
    tick.symbol = raw.substr(0, pos);
    tick.last_price = std::stod(raw.substr(pos + 1));
    tick.ts = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    // 回调下游（数据分发/策略等）。
    cb(tick);
}

} // namespace qf

