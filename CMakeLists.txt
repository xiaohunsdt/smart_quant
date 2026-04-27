cmake_minimum_required(VERSION 3.24)

# ── Conan 2 Dependency Provider (injection) ────────────────────────────────────
# cmake/conan_provider.cmake intercepts every find_package() call and
# automatically runs `conan install` before CMake tries to locate the package.
# This means you NEVER need to run `conan install` manually or pass
# -DCMAKE_TOOLCHAIN_FILE on the command line.
#
# Build from scratch with just:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j$(nproc)
#
# The provider is only activated when this is the top-level project
# (CMAKE_PROJECT_TOP_LEVEL_INCLUDES is ignored in sub-projects).
set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/conan_provider.cmake")

project(smart_quant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler flags ─────────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-O0>
)

# ── Dependencies — resolved automatically by conan_provider.cmake ──────────────
find_package(quickfix REQUIRED)
find_package(spdlog   REQUIRED)
find_package(Catch2 3 REQUIRED)

# ── Shared static library (all modules) ───────────────────────────────────────
add_library(sq_core STATIC
    src/common/tsc_clock.cpp
    src/log/binary_logger.cpp
    src/log/lob_event_logger.cpp
    src/md/order_book.cpp
    src/md/fix_md_gateway.cpp
    src/alpha/delta_factor.cpp
    src/alpha/cancel_factor.cpp
    src/alpha/signal_engine.cpp
    src/oms/order_manager.cpp
    src/risk/risk_manager.cpp
)

target_include_directories(sq_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(sq_core PUBLIC
    quickfix::quickfix
    spdlog::spdlog
)

# ── Executables ────────────────────────────────────────────────────────────────
add_executable(sq_logger   apps/logger/main.cpp)
add_executable(sq_backtest apps/backtest/main.cpp)
add_executable(sq_engine   apps/engine/main.cpp)

target_link_libraries(sq_logger   PRIVATE sq_core)
target_link_libraries(sq_backtest PRIVATE sq_core)
target_link_libraries(sq_engine   PRIVATE sq_core)

# ── Tests ──────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(sq_tests
    tests/test_order_book.cpp
    tests/test_alpha.cpp
    tests/test_risk.cpp
)
target_link_libraries(sq_tests PRIVATE sq_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(sq_tests)
