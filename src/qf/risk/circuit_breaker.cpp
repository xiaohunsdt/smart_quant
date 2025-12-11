#include "qf/risk/circuit_breaker.hpp"
#include "qf/infrastructure/logger.hpp"

namespace qf {

void CircuitBreaker::trip() {
    tripped_.store(true);
    Logger::instance().error("Circuit breaker TRIPPED");
}

void CircuitBreaker::reset() {
    tripped_.store(false);
    Logger::instance().info("Circuit breaker RESET");
}

bool CircuitBreaker::tripped() const { return tripped_.load(); }

} // namespace qf

