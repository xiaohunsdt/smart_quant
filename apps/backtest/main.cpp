#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "smartquant/common/types.hpp"
#include "smartquant/log/lob_event_logger.hpp"
#include "smartquant/md/order_book.hpp"
#include "smartquant/alpha/signal_engine.hpp"

// ── Backtest result accumulator ───────────────────────────────────────────────

struct BacktestResult {
    uint64_t total_signals{0};
    uint64_t buy_signals{0};
    uint64_t sell_signals{0};

    // Win = price moved ≥ 1 tick in predicted direction within 5 s
    uint64_t wins{0};
    uint64_t losses{0};

    // Single-factor (delta only) tracking
    uint64_t sf_wins{0};
    uint64_t sf_total{0};

    double total_move_ticks{0.0};

    double win_rate()    const { return total_signals > 0 ? 100.0 * wins / total_signals : 0.0; }
    double sf_win_rate() const { return sf_total > 0 ? 100.0 * sf_wins / sf_total : 0.0; }
    double avg_move()    const { return total_signals > 0 ? total_move_ticks / total_signals : 0.0; }
};

// Pending signal for forward outcome evaluation
struct PendingSignal {
    sq::Signal sig;
    sq::Price  entry_price;   // L1 at signal time
    // Single-factor signal fired at same tick?
    bool sf_fired{false};
};

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <path/to/file.fixlog> [thresh_delta=50] [output.csv]\n";
        return EXIT_FAILURE;
    }

    const std::string log_path     = argv[1];
    const double      thresh_delta = (argc >= 3) ? std::stod(argv[2]) : 50.0;
    const std::string csv_path     = (argc >= 4) ? argv[3] : "";

    auto console = spdlog::stdout_color_mt("backtest");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("Loading LOB log: {}", log_path);

    sq::LobEventLogger::Reader reader(log_path);
    spdlog::info("Records available: {}", reader.total_records());

    sq::OrderBook  book;
    sq::SignalEngine engine(thresh_delta);

    // Single-factor engine (delta only, cancel threshold set very high)
    sq::SignalEngine sf_engine(thresh_delta, 1e9 /* cancel never fires */);

    BacktestResult result;
    std::vector<PendingSignal> pending;

    // Price history for forward outcome lookup
    struct PricePoint { uint64_t ts_ns; sq::Price bid; sq::Price ask; };
    std::vector<PricePoint> price_history;
    price_history.reserve(1 << 20);

    static constexpr uint64_t kLookForwardNs = 5'000'000'000ULL;  // 5 seconds

    sq::LOBEvent ev{};
    uint64_t rec = 0;

    while (reader.next(ev)) {
        ++rec;
        if (rec % 500'000 == 0)
            spdlog::info("Processed {} records ...", rec);

        // Apply event to order book
        book.update(ev);

        // Record L1 price point after book update
        if (book.best_bid() > 0 && book.best_ask() > 0)
            price_history.push_back({ev.timestamp_ns,
                                     book.best_bid(),
                                     book.best_ask()});

        // Evaluate pending signals whose 5-second window has elapsed
        if (!pending.empty()) {
            auto it = pending.begin();
            while (it != pending.end()) {
                if (ev.timestamp_ns - it->sig.trigger_time_ns >= kLookForwardNs) {
                    const uint64_t target_ts =
                        it->sig.trigger_time_ns + kLookForwardNs;
                    auto pit = std::lower_bound(
                        price_history.begin(), price_history.end(),
                        target_ts,
                        [](const PricePoint& p, uint64_t t){ return p.ts_ns < t; });

                    if (pit != price_history.end()) {
                        double move_ticks = 0.0;
                        bool win = false;
                        if (it->sig.direction == sq::Direction::Buy) {
                            const sq::Price diff = pit->ask - it->entry_price;
                            move_ticks = static_cast<double>(diff) / sq::kPriceScale;
                            win = (diff >= sq::kPriceScale);
                        } else {
                            const sq::Price diff = it->entry_price - pit->bid;
                            move_ticks = static_cast<double>(diff) / sq::kPriceScale;
                            win = (diff >= sq::kPriceScale);
                        }
                        result.total_move_ticks += move_ticks;
                        if (win) ++result.wins; else ++result.losses;
                        if (it->sf_fired) {
                            ++result.sf_total;
                            if (win) ++result.sf_wins;
                        }
                    }
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Run dual-factor engine
        if (engine.on_event(ev, book)) {
            const sq::Signal& sig = engine.last_signal();
            ++result.total_signals;
            if (sig.direction == sq::Direction::Buy)  ++result.buy_signals;
            if (sig.direction == sq::Direction::Sell) ++result.sell_signals;
            const bool sf_fired = sf_engine.on_event(ev, book);
            pending.push_back({sig, sig.l1_price, sf_fired});
        } else {
            sf_engine.on_event(ev, book);
        }
    }

    // ── Print report ──────────────────────────────────────────────────────────
    spdlog::info("=== Backtest Complete ===");
    spdlog::info("Records processed : {}", rec);
    spdlog::info("Total signals      : {} (BUY={} SELL={})",
                 result.total_signals, result.buy_signals, result.sell_signals);
    spdlog::info("Dual-factor win rate: {:.1f}%  ({}/{} evaluated)",
                 result.win_rate(), result.wins, result.wins + result.losses);
    spdlog::info("Single-factor winrate:{:.1f}%  ({}/{} evaluated)",
                 result.sf_win_rate(), result.sf_wins, result.sf_total);
    spdlog::info("Avg price move     : {:.3f} ticks", result.avg_move());
    spdlog::info("Dual vs single improvement: {:.1f}pp",
                 result.win_rate() - result.sf_win_rate());

    // ── Optional CSV output ───────────────────────────────────────────────────
    if (!csv_path.empty()) {
        std::ofstream csv(csv_path);
        csv << "metric,value\n"
            << "records," << rec << "\n"
            << "total_signals," << result.total_signals << "\n"
            << "buy_signals,"   << result.buy_signals   << "\n"
            << "sell_signals,"  << result.sell_signals  << "\n"
            << "wins,"          << result.wins          << "\n"
            << "losses,"        << result.losses        << "\n"
            << "win_rate_pct,"  << result.win_rate()    << "\n"
            << "sf_wins,"       << result.sf_wins       << "\n"
            << "sf_total,"      << result.sf_total      << "\n"
            << "sf_win_rate_pct,"<< result.sf_win_rate()<< "\n"
            << "avg_move_ticks,"<< result.avg_move()    << "\n";
        spdlog::info("CSV written to: {}", csv_path);
    }

    return EXIT_SUCCESS;
}
