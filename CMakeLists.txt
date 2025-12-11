cmake_minimum_required(VERSION 3.16)

project(quant_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL)
find_package(yaml-cpp)
find_package(spdlog)

add_library(qf STATIC
    src/qf/infrastructure/logger.cpp
    src/qf/infrastructure/config.cpp
    src/qf/infrastructure/thread_pool.cpp
    src/qf/common/http_client.cpp
    src/qf/data/data_types.cpp
    src/qf/data/data_source.cpp
    src/qf/data/data_parser.cpp
    src/qf/data/data_distributor.cpp
    src/qf/data/market_cache.cpp
    src/qf/data/historical_loader.cpp
    src/qf/strategy/strategy_base.cpp
    src/qf/strategy/strategy_factory.cpp
    src/qf/strategy/strategy_scheduler.cpp
    src/qf/strategy/signal_generator.cpp
    src/qf/strategy/strategy_configurator.cpp
    src/qf/trading/trading_interface.cpp
    src/qf/trading/order_manager.cpp
    src/qf/trading/position_manager.cpp
    src/qf/trading/order_generator.cpp
    src/qf/trading/report_handler.cpp
    src/qf/trading/account_manager.cpp
    src/qf/risk/risk_engine.cpp
    src/qf/risk/risk_metrics.cpp
    src/qf/risk/circuit_breaker.cpp
    src/qf/risk/compliance_checker.cpp
    src/qf/interface/api_server.cpp
    src/sources/dummy_data_source.cpp
    src/sources/binance_data_source.cpp
    src/strategies/sample_strategy.cpp
    src/trade/dummy_trading_interface.cpp
)

target_include_directories(qf PUBLIC include)
target_link_libraries(qf PUBLIC spdlog::spdlog yaml-cpp::yaml-cpp CURL::libcurl)

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

add_executable(smart_quant src/main.cpp)
target_link_libraries(smart_quant PRIVATE qf)

