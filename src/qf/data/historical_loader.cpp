#include "qf/data/historical_loader.hpp"
#include <fstream>

namespace qf {

void HistoricalLoader::load_and_replay(const std::string& path, ReplayCallback cb) const {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        TickData tick;
        tick.symbol = "replay";
        tick.last_price = std::stod(line);
        tick.ts = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
        // 与实时数据相同的回调接口，方便复用策略逻辑。
        cb(tick);
    }
}

} // namespace qf

