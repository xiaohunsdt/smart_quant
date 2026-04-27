#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

#include <sched.h>   // sched_setaffinity
#include <unistd.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "smartquant/common/tsc_clock.hpp"
#include "smartquant/log/lob_event_logger.hpp"
#include "smartquant/md/fix_md_gateway.hpp"
#include "smartquant/alpha/signal_engine.hpp"
#include "smartquant/oms/order_manager.hpp"
#include "smartquant/risk/risk_manager.hpp"

// ── Core affinity ─────────────────────────────────────────────────────────────
static bool pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return ::pthread_setaffinity_np(::pthread_self(), sizeof(cpuset), &cpuset) == 0;
}

// ── Signal handling ───────────────────────────────────────────────────────────
static std::atomic<bool> g_running{true};

static void on_signal(int sig) noexcept {
    spdlog::warn("Received signal {} — initiating shutdown", sig);
    g_running.store(false, std::memory_order_relaxed);
}

// ── Alpha thread (reads SPSC queue, runs signal engine, sends orders) ─────────
static void alpha_thread_fn(sq::MdQueue&       queue,
                             sq::OrderBook&     book,
                             sq::SignalEngine&  engine,
                             sq::OrderManager&  oms,
                             sq::RiskManager&   risk,
                             int                core_id) {
    if (!pin_to_core(core_id))
        spdlog::warn("Alpha thread: failed to pin to core {}", core_id);
    else
        spdlog::info("Alpha thread pinned to core {}", core_id);

    sq::LOBEvent ev{};

    while (g_running.load(std::memory_order_relaxed)) {
        if (!queue.pop(ev)) {
            // Busy-spin on empty queue (zero latency, burns CPU — acceptable on
            // an isolated core in production)
            continue;
        }

        const uint64_t now_ns = sq::TscClock::now_ns();

        // Position timeout check
        risk.on_timer(now_ns, [&](const sq::Order& o) {
            spdlog::warn("Force-closing timed-out position: {}", o.clordid);
            oms.cancel_order(o.clordid);
            // Send closing market order
            const sq::Side close_side =
                (o.side == sq::Side::Buy) ? sq::Side::Sell : sq::Side::Buy;
            oms.send_ioc(close_side, o.qty,
                         (o.side == sq::Side::Buy)
                             ? book.best_bid()
                             : book.best_ask(),
                         now_ns);
        });

        // Run dual-factor signal engine
        if (!engine.on_event(ev, book)) continue;

        const sq::Signal& sig = engine.last_signal();

        if (!risk.check_signal(sig, now_ns)) continue;

        // Fire IOC order
        const sq::Qty  qty = 1;   // 0.01 lot pilot — adjust as needed
        oms.send_ioc(sig.direction == sq::Direction::Buy
                         ? sq::Side::Buy : sq::Side::Sell,
                     qty,
                     sig.l1_price,
                     now_ns);
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <fix_md.cfg> <fix_trade.cfg> [log_dir] [alpha_core=2] [md_core=3]\n";
        return EXIT_FAILURE;
    }

    const std::string md_cfg      = argv[1];
    const std::string trade_cfg   = argv[2];
    const std::string log_dir     = (argc >= 4) ? argv[3] : "./logs";
    const int         alpha_core  = (argc >= 5) ? std::stoi(argv[4]) : 2;
    (void)(argc >= 6 ? std::stoi(argv[5]) : 3);  // md_core reserved for future use

    std::signal(SIGPIPE, SIG_IGN);  // broker disconnect must not kill the process
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    auto console = spdlog::stdout_color_mt("engine");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    try {
        // ── 1. Calibrate TSC ──────────────────────────────────────────────────
        spdlog::info("Calibrating TSC clock ...");
        sq::TscClock::calibrate(20);
        spdlog::info("TSC freq = {:.4f} GHz", sq::TscClock::freq_ghz());

        // ── 2. LOB Logger ─────────────────────────────────────────────────────
        std::filesystem::create_directories(log_dir);
        const std::string log_path =
            log_dir + "/xauusd_" +
            std::to_string(sq::TscClock::now_ns() / 1'000'000'000ULL) + ".loblog";
        spdlog::info("LOB log: {}", log_path);
        sq::LobEventLogger binary_logger(log_path);

        // ── 3. Construct components ───────────────────────────────────────────
        sq::MdQueue       md_queue;
        sq::RiskManager   risk_mgr;
        sq::OrderManager  oms(trade_cfg, risk_mgr);
        sq::SignalEngine  engine;

        // Wire fill/reject callbacks from OMS → RiskManager (already wired
        // inside OrderManager, but we can add spdlog callbacks here)
        oms.set_fill_callback([](const sq::Order& o) {
            spdlog::info("ORDER FILLED: {} fill={:.2f}",
                         o.clordid, sq::from_price(o.fill_price));
        });
        oms.set_reject_callback([](const sq::Order& o) {
            spdlog::warn("ORDER REJECTED/EXPIRED: {}", o.clordid);
        });

        // ── 4. MD gateway (runs on md_core via QuickFIX internal thread) ─────
        sq::FixMdGateway md_gw(md_cfg, binary_logger, &md_queue);

        // ── 5. Start OMS FIX session ──────────────────────────────────────────
        spdlog::info("Starting trade FIX session ...");
        oms.start();

        // ── 6. Start Alpha thread pinned to alpha_core ───────────────────────
        spdlog::info("Starting alpha thread on core {} ...", alpha_core);
        std::thread alpha_thr(alpha_thread_fn,
                              std::ref(md_queue),
                              std::ref(md_gw.order_book()),
                              std::ref(engine),
                              std::ref(oms),
                              std::ref(risk_mgr),
                              alpha_core);

        // ── 7. Start MD FIX session ───────────────────────────────────────────
        spdlog::info("Starting market data FIX session ...");
        md_gw.start();

        spdlog::info("Engine running — press Ctrl+C to stop");

        // Main loop: periodic housekeeping
        while (g_running.load(std::memory_order_relaxed)) {
            ::sleep(1);
            binary_logger.flush();

            spdlog::info("daily_pnl={:.2f}  trades={}  fused={}",
                         risk_mgr.daily_pnl_usd(),
                         risk_mgr.total_trades(),
                         risk_mgr.is_fused());
        }

        // ── 8. Graceful shutdown ──────────────────────────────────────────────
        spdlog::info("Stopping ...");
        md_gw.stop();
        oms.stop();
        alpha_thr.join();
        binary_logger.flush();
        spdlog::info("Shutdown complete. Total log: {} records ({:.2f} MiB)",
                     binary_logger.total_written(),
                     static_cast<double>(binary_logger.bytes_written()) / (1 << 20));

    } catch (const std::exception& ex) {
        spdlog::error("Fatal: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
