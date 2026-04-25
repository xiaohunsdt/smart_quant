#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "smartquant/alpha/delta_factor.hpp"
#include "smartquant/alpha/cancel_factor.hpp"
#include "smartquant/alpha/signal_engine.hpp"
#include "smartquant/md/order_book.hpp"

using namespace sq;

static LOBEvent trade_ev(double price, Qty qty, uint64_t ts_ns) {
    return LOBEvent{ts_ns, to_price(price), qty, MDUpdateAction::New, MDEntryType::Trade};
}

static LOBEvent cancel_ev(MDEntryType side, double price, uint64_t ts_ns) {
    return LOBEvent{ts_ns, to_price(price), 0, MDUpdateAction::Delete, side};
}

// ── DeltaFactor ───────────────────────────────────────────────────────────────
TEST_CASE("DeltaFactor: no events gives zero", "[delta]") {
    DeltaFactor df;
    REQUIRE(df.value() == Catch::Approx(0.0));
    REQUIRE(df.net_delta() == Catch::Approx(0.0));
}

TEST_CASE("DeltaFactor: buy trades increase net_delta", "[delta]") {
    DeltaFactor df;
    // Midprice not set → all trades classified as buys (price >= 0 == midprice)
    df.on_event(trade_ev(2000.0, 10, 1'000'000));
    df.on_event(trade_ev(2000.0, 10, 2'000'000));
    REQUIRE(df.net_delta() > 0.0);
}

TEST_CASE("DeltaFactor: sell trades decrease net_delta", "[delta]") {
    DeltaFactor df;
    df.set_midprice(to_price(2000.0));
    // Trades below midprice = sells
    df.on_event(trade_ev(1999.0, 10, 1'000'000));
    df.on_event(trade_ev(1999.0, 10, 2'000'000));
    REQUIRE(df.net_delta() < 0.0);
}

TEST_CASE("DeltaFactor: reset clears state", "[delta]") {
    DeltaFactor df;
    df.on_event(trade_ev(2000.0, 100, 1'000'000));
    df.reset();
    REQUIRE(df.value()     == Catch::Approx(0.0));
    REQUIRE(df.net_delta() == Catch::Approx(0.0));
}

TEST_CASE("DeltaFactor: acceleration computed after 3+ windows", "[delta]") {
    DeltaFactor df(100'000'000ULL, 10'000'000ULL);  // 100ms window, 10ms buckets
    df.set_midprice(to_price(2000.0));

    uint64_t ts = 0;
    // Inject trades in increasing volumes across buckets → rising delta
    for (int bucket = 0; bucket < 5; ++bucket) {
        ts += 15'000'000ULL;  // 15 ms per bucket
        df.on_event(trade_ev(2000.1, (bucket + 1) * 10, ts));
    }
    // After 3 delta samples the acceleration should be non-zero
    REQUIRE(df.value() != Catch::Approx(0.0).margin(1e-9));
}

// ── CancelFactor ─────────────────────────────────────────────────────────────
TEST_CASE("CancelFactor: no events gives zero z-scores", "[cancel]") {
    CancelFactor cf;
    REQUIRE(cf.z_bid() == Catch::Approx(0.0));
    REQUIRE(cf.z_ask() == Catch::Approx(0.0));
}

TEST_CASE("CancelFactor: ask cancels increase z_ask", "[cancel]") {
    CancelFactor cf;
    uint64_t ts = 0;

    // Build up baseline
    for (int i = 0; i < 20; ++i) {
        ts += 12'000'000ULL;
        cf.on_event(cancel_ev(MDEntryType::Offer, 2001.0, ts));
    }

    // Burst of ask cancels
    for (int i = 0; i < 10; ++i) {
        ts += 1'000'000ULL;
        cf.on_event(cancel_ev(MDEntryType::Offer, 2001.0, ts));
    }

    // z_ask should be elevated
    REQUIRE(cf.z_ask() > 0.0);
}

TEST_CASE("CancelFactor: bid cancels increase z_bid", "[cancel]") {
    CancelFactor cf;
    uint64_t ts = 0;

    for (int i = 0; i < 20; ++i) {
        ts += 12'000'000ULL;
        cf.on_event(cancel_ev(MDEntryType::Bid, 2000.0, ts));
    }
    for (int i = 0; i < 10; ++i) {
        ts += 1'000'000ULL;
        cf.on_event(cancel_ev(MDEntryType::Bid, 2000.0, ts));
    }

    REQUIRE(cf.z_bid() > 0.0);
}

TEST_CASE("CancelFactor: reset clears state", "[cancel]") {
    CancelFactor cf;
    cf.on_event(cancel_ev(MDEntryType::Bid, 2000.0, 1'000'000));
    cf.reset();
    REQUIRE(cf.z_bid() == Catch::Approx(0.0));
    REQUIRE(cf.z_ask() == Catch::Approx(0.0));
}

// ── SignalEngine ──────────────────────────────────────────────────────────────
TEST_CASE("SignalEngine: no signal on sparse data", "[signal]") {
    OrderBook book;
    SignalEngine engine(50.0, 1.5);

    book.update({0, to_price(2000.0), 10, MDUpdateAction::New, MDEntryType::Bid});
    book.update({0, to_price(2001.0), 10, MDUpdateAction::New, MDEntryType::Offer});

    LOBEvent ev{1'000'000, to_price(2000.5), 1, MDUpdateAction::New, MDEntryType::Trade};
    bool fired = engine.on_event(ev, book);
    REQUIRE(!fired);
    REQUIRE(engine.last_signal().direction == Direction::None);
}

TEST_CASE("SignalEngine: reset clears last signal", "[signal]") {
    SignalEngine engine;
    engine.reset();
    REQUIRE(engine.last_signal().direction == Direction::None);
}
