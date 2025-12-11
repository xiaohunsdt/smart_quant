#include "qf/risk/circuit_breaker.hpp"
#include "qf/infrastructure/logger.hpp"

namespace qf {

void CircuitBreaker::trip() {
    tripped_.store(true);
    Logger::instance().error("熔断器已触发");
}

void CircuitBreaker::reset() {
    tripped_.store(false);
    Logger::instance().info("熔断器已重置");
}

bool CircuitBreaker::tripped() const { return tripped_.load(); }

} // namespace qf

