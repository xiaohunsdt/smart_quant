#pragma once

#include <string>
#include <optional>
#include <cstddef>

namespace qf {

// 简单的 HTTP GET 客户端封装（基于 libcurl）。
class HttpClient {
public:
    // 发送 GET 请求，超时单位毫秒，返回响应字符串或空。
    std::optional<std::string> get(const std::string& url, std::size_t timeout_ms = 2000);
};

} // namespace qf

