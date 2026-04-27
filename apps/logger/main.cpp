#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "smartquant/common/tsc_clock.hpp"
#include "smartquant/log/lob_event_logger.hpp"
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

        // Create output log file with timestamp in name.
        // Format: compact binary LOBEvents (40 bytes each), no raw FIX text.
        std::filesystem::create_directories(output_dir);
        const uint64_t ts_now = sq::TscClock::now_ns();
        const std::string log_path =
            output_dir + "/xauusd_" + std::to_string(ts_now / 1'000'000'000ULL) + ".loblog";

        // Default capacity: 4 Mi records ≈ 160 MiB, holds several hours of LOB data.
        spdlog::info("Opening LOB log: {}", log_path);
        sq::LobEventLogger lob_logger(log_path);

        // Start FIX MD gateway
        spdlog::info("Loading FIX config: {}", fix_cfg);
        sq::FixMdGateway gateway(fix_cfg, lob_logger, nullptr /* no alpha queue */);
        gateway.start();

        spdlog::info("sq_logger running — press Ctrl+C to stop");

        while (g_running.load(std::memory_order_relaxed)) {
            ::usleep(1'000'000);
            lob_logger.flush();

            // Abort early on auth failure — no point retrying with wrong creds
            if (gateway.auth_failed()) {
                spdlog::error("Stopping: authentication failure. "
                              "Fix credentials in fix_market_data.cfg and restart.");
                g_running.store(false);
                break;
            }

            const uint64_t records = lob_logger.total_written();
            const uint64_t bytes   = lob_logger.bytes_written();
            spdlog::info("Written: {:.2f} MiB ({} records)",
                         static_cast<double>(bytes) / (1 << 20), records);
        }

        spdlog::info("Shutting down ...");
        gateway.stop();
        lob_logger.flush();
        spdlog::info("Done. Total written: {} records ({:.2f} MiB)",
                     lob_logger.total_written(),
                     static_cast<double>(lob_logger.bytes_written()) / (1 << 20));

    } catch (const std::exception& ex) {
        spdlog::error("Fatal: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
