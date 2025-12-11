#include "qf/interface/api_server.hpp"

namespace qf {

void ApiServer::start() { running_ = true; }
void ApiServer::stop() { running_ = false; }
std::string ApiServer::status() const { return running_ ? "running" : "stopped"; }

} // namespace qf

