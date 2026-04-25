#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "smartquant/risk/risk_manager.hpp"

using namespace sq;

static Signal make_signal(Direction dir, Price l1 = to_price(2000.0),
                          uint64_t ts = 1'000'000'000ULL) {
    return Signal{ts, l1, 100.0, 2.5, dir};
}

static Order make_filled_order(Side side, Price signal_l1, Price fill_price,
                               uint64_t ts = 2'000'000'000ULL) {
    Order o{};
    o.side       = side;
    o.signal_l1  = signal_l1;
    o.fill_price = fill_price;
    o.qty        = 1;
    o.sent_ts_ns = ts - 1'000'000;
    o.fill_ts_ns = ts;
    o.status     = OrderStatus::Filled;
    return o;
}

TEST_CASE("RiskManager: fresh instance allows signal", "[risk]") {
    RiskManager rm;
    Signal sig = make_signal(Direction::Buy);
    REQUIRE(rm.check_signal(sig, 1'000'000'000ULL));
}

TEST_CASE("RiskManager: blocks second signal when position open", "[risk]") {
    RiskManager rm;
    Signal sig = make_signal(Direction::Buy);
    Order o{};
    o.side = Side::Buy; o.qty = 1;

    rm.on_order_sent(o, 1'000'000'000ULL);
    REQUIRE(!rm.check_signal(sig, 1'000'000'000ULL));
}

TEST_CASE("RiskManager: allows signal after fill", "[risk]") {
    RiskManager rm;
    const uint64_t t0 = 1'000'000'000ULL;

    Order sent{};
    sent.side = Side::Buy; sent.qty = 1;
    rm.on_order_sent(sent, t0);

    Order filled = make_filled_order(Side::Buy,
                                     to_price(2000.0), to_price(2000.05));
    rm.on_fill(filled, t0 + 1'000'000);

    Signal sig = make_signal(Direction::Sell, to_price(2001.0), t0 + 2'000'000);
    REQUIRE(rm.check_signal(sig, t0 + 2'000'000));
}

TEST_CASE("RiskManager: cooldown after excessive consecutive slippage", "[risk]") {
    RiskManager::Config cfg;
    cfg.cooldown_slip_ticks = 2.0;
    cfg.cooldown_consec_slips = 3;
    cfg.cooldown_duration_ns = 60'000'000'000ULL;
    RiskManager rm(cfg);

    const uint64_t base = 1'000'000'000ULL;

    for (int i = 0; i < 3; ++i) {
        Order sent{};
        sent.side = Side::Buy; sent.qty = 1;
        rm.on_order_sent(sent, base + i * 100'000'000ULL);

        // 3-tick slippage (above 2-tick threshold)
        Order filled = make_filled_order(Side::Buy,
            to_price(2000.0), to_price(2003.0),
            base + i * 100'000'000ULL + 50'000'000ULL);
        rm.on_fill(filled, base + i * 100'000'000ULL + 50'000'000ULL);
    }

    const uint64_t now = base + 500'000'000ULL;
    REQUIRE(rm.in_cooldown(now));

    Signal sig = make_signal(Direction::Buy);
    REQUIRE(!rm.check_signal(sig, now));
}

TEST_CASE("RiskManager: fuse trips on daily loss limit", "[risk]") {
    RiskManager::Config cfg;
    cfg.max_daily_loss_usd = 10.0;
    cfg.tick_size_usd      = 1.0;
    // single_slip_abort high so we get to the cumulative path
    cfg.single_slip_abort_ticks = 100.0;
    cfg.cooldown_slip_ticks     = 0.0;  // every fill adds to consec count
    cfg.cooldown_consec_slips   = 1000; // high — don't want cooldown here
    RiskManager rm(cfg);

    const uint64_t base = 1'000'000'000ULL;

    // Each fill costs 15 ticks × $1/tick = $15 → exceeds $10 limit after 1 fill
    Order sent{};
    sent.side = Side::Buy; sent.qty = 1;
    rm.on_order_sent(sent, base);

    Order filled = make_filled_order(Side::Buy,
        to_price(2000.0), to_price(2015.0), base + 1'000'000);
    rm.on_fill(filled, base + 1'000'000);

    REQUIRE(rm.is_fused());
    REQUIRE(!rm.check_signal(make_signal(Direction::Sell), base + 2'000'000));
}

TEST_CASE("RiskManager: reset_daily clears fuse and pnl", "[risk]") {
    RiskManager::Config cfg;
    cfg.max_daily_loss_usd      = 5.0;
    cfg.tick_size_usd           = 1.0;
    cfg.single_slip_abort_ticks = 100.0;
    RiskManager rm(cfg);

    const uint64_t base = 1'000'000'000ULL;
    Order sent{};
    sent.side = Side::Buy; sent.qty = 1;
    rm.on_order_sent(sent, base);
    Order filled = make_filled_order(Side::Buy,
        to_price(2000.0), to_price(2010.0), base + 1'000'000);
    rm.on_fill(filled, base + 1'000'000);

    REQUIRE(rm.is_fused());
    rm.reset_daily();
    REQUIRE(!rm.is_fused());
    REQUIRE(rm.daily_pnl_usd() == Catch::Approx(0.0));
    REQUIRE(rm.check_signal(make_signal(Direction::Buy), base + 2'000'000));
}

TEST_CASE("RiskManager: not in cooldown initially", "[risk]") {
    RiskManager rm;
    REQUIRE(!rm.in_cooldown(1'000'000'000ULL));
}

TEST_CASE("RiskManager: cooldown expires", "[risk]") {
    RiskManager::Config cfg;
    cfg.cooldown_slip_ticks     = 1.0;
    cfg.cooldown_consec_slips   = 1;
    cfg.cooldown_duration_ns    = 1'000'000'000ULL;  // 1 second
    cfg.single_slip_abort_ticks = 100.0;
    RiskManager rm(cfg);

    const uint64_t base = 1'000'000'000ULL;
    Order sent{};
    sent.side = Side::Buy; sent.qty = 1;
    rm.on_order_sent(sent, base);
    Order filled = make_filled_order(Side::Buy,
        to_price(2000.0), to_price(2002.0), base + 50'000'000);
    rm.on_fill(filled, base + 50'000'000);

    REQUIRE(rm.in_cooldown(base + 500'000'000));
    // After 1 second cooldown expires
    REQUIRE(!rm.in_cooldown(base + 2'000'000'000ULL));
}
