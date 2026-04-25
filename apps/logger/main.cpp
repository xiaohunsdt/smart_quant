#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "smartquant/common/tsc_clock.hpp"
#include "smartquant/log/binary_logger.hpp"
#include "smartquant/md/fix_md_gateway.hpp"

// ── Signal handling ───────────────────────────────────────────────────────────
static std::atomic<bool> g_running{true};

static void on_signal(int) noexcept {
    g_running.store(false, std::memory_order_relaxed);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Usage: sq_logger <fix_config> [output_dir]
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <fix_market_data.cfg> [output_dir]\n";
        return EXIT_FAILURE;
    }

    const std::string fix_cfg    = argv[1];
    const std::string output_dir = (argc >= 3) ? argv[2] : ".";

    // Ignore SIGPIPE — prevents crash when the broker closes the TCP socket
    // while QuickFIX is still writing (broken-pipe condition).
    // All socket errors are handled through QuickFIX error callbacks instead.
    std::signal(SIGPIPE, SIG_IGN);

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    auto logger = spdlog::stdout_color_mt("sq_logger");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    try {
        // Calibrate TSC clock
        spdlog::info("Calibrating TSC clock ...");
        sq::TscClock::calibrate(20);
        spdlog::info("TSC freq = {:.4f} GHz", sq::TscClock::freq_ghz());

        // Create output log file with timestamp in name
        std::filesystem::create_directories(output_dir);
        const uint64_t ts_now = sq::TscClock::now_ns();
        const std::string log_path =
            output_dir + "/xauusd_" + std::to_string(ts_now / 1'000'000'000ULL) + ".fixlog";

        spdlog::info("Opening binary log: {}", log_path);
        sq::BinaryLogger binary_logger(log_path, 512ULL << 20 /* 512 MiB */);

        // Start FIX MD gateway
        spdlog::info("Loading FIX config: {}", fix_cfg);
        sq::FixMdGateway gateway(fix_cfg, binary_logger, nullptr /* no alpha queue */);
        gateway.start();

        spdlog::info("sq_logger running — press Ctrl+C to stop");

        while (g_running.load(std::memory_order_relaxed)) {
            // Flush binary log every second
            ::usleep(1'000'000);
            binary_logger.flush();

            const uint64_t written = binary_logger.total_written();
            spdlog::info("Written: {:.2f} MiB ({} bytes)", static_cast<double>(written) / (1 << 20), written);
        }

        spdlog::info("Shutting down ...");
        gateway.stop();
        binary_logger.flush();
        spdlog::info("Done. Total written: {} bytes", binary_logger.total_written());

    } catch (const std::exception& ex) {
        spdlog::error("Fatal: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
