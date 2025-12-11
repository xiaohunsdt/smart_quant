#include "qf/common/http_client.hpp"
#include <curl/curl.h>

namespace qf {

namespace {
size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
}

std::optional<std::string> HttpClient::get(const std::string& url, std::size_t timeout_ms) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    std::string buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        return std::nullopt;
    }
    return buffer;
}

} // namespace qf

