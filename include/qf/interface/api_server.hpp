#pragma once

#include <string>

namespace qf {

class ApiServer {
public:
    // 启动对外接口（示例为空实现）。
    void start();
    void stop();
    // 返回当前状态。
    std::string status() const;

private:
    bool running_{false};
};

} // namespace qf

