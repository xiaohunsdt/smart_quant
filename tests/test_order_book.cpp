#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "smartquant/md/order_book.hpp"

using namespace sq;

// Build a New/Change event with a given entry_id.
static LOBEvent make_ev(uint64_t id, MDUpdateAction act, MDEntryType side,
                        double price, Qty qty, uint64_t ts = 1000) {
    LOBEvent ev{};
    ev.timestamp_ns = ts;
    ev.entry_id     = id;
    ev.action       = act;
    ev.side         = side;
    ev.price        = to_price(price);
    ev.qty          = qty;
    return ev;
}

// Build a Delete event: only entry_id matters (side/price resolved from book).
static LOBEvent make_del(uint64_t id, uint64_t ts = 1000) {
    LOBEvent ev{};
    ev.timestamp_ns = ts;
    ev.entry_id     = id;
    ev.action       = MDUpdateAction::Delete;
    return ev;
}

TEST_CASE("OrderBook: empty book", "[order_book]") {
    OrderBook book;
    REQUIRE(book.empty());
    REQUIRE(book.best_bid() == 0);
    REQUIRE(book.best_ask() == 0);
    REQUIRE(book.spread() == 0);
}

TEST_CASE("OrderBook: add bid and ask", "[order_book]") {
    OrderBook book;
    book.update(make_ev(1, MDUpdateAction::New, MDEntryType::Bid,   2000.50, 10));
    book.update(make_ev(2, MDUpdateAction::New, MDEntryType::Offer, 2000.60, 5));

    REQUIRE(book.best_bid() == to_price(2000.50));
    REQUIRE(book.best_ask() == to_price(2000.60));
    REQUIRE(book.spread()   == to_price(2000.60) - to_price(2000.50));
    REQUIRE(!book.empty());
}

TEST_CASE("OrderBook: best bid is highest bid price", "[order_book]") {
    OrderBook book;
    book.update(make_ev(1, MDUpdateAction::New, MDEntryType::Bid, 1990.00, 5));
    book.update(make_ev(2, MDUpdateAction::New, MDEntryType::Bid, 1995.00, 3));
    book.update(make_ev(3, MDUpdateAction::New, MDEntryType::Bid, 1993.00, 8));

    REQUIRE(book.best_bid() == to_price(1995.00));
}

TEST_CASE("OrderBook: best ask is lowest ask price", "[order_book]") {
    OrderBook book;
    book.update(make_ev(1, MDUpdateAction::New, MDEntryType::Offer, 2005.00, 5));
    book.update(make_ev(2, MDUpdateAction::New, MDEntryType::Offer, 2001.00, 3));
    book.update(make_ev(3, MDUpdateAction::New, MDEntryType::Offer, 2003.00, 8));

    REQUIRE(book.best_ask() == to_price(2001.00));
}

TEST_CASE("OrderBook: delete removes level (L3 entry-id semantics)", "[order_book]") {
    OrderBook book;
    // Two entries at different price levels
    book.update(make_ev(10, MDUpdateAction::New, MDEntryType::Bid, 2000.00, 10));
    book.update(make_ev(11, MDUpdateAction::New, MDEntryType::Bid, 1999.00, 5));
    // Delete by entry_id — no price/side needed
    book.update(make_del(10));

    REQUIRE(book.best_bid() == to_price(1999.00));
    REQUIRE(book.bid_depth() == 1);
}

TEST_CASE("OrderBook: two entries at same price level aggregate qty", "[order_book]") {
    OrderBook book;
    book.update(make_ev(20, MDUpdateAction::New, MDEntryType::Offer, 2001.00, 10));
    book.update(make_ev(21, MDUpdateAction::New, MDEntryType::Offer, 2001.00, 5));

    REQUIRE(book.ask_qty_at(to_price(2001.00)) == 15);

    // Delete one entry — level still exists at reduced qty
    book.update(make_del(20));
    REQUIRE(book.ask_qty_at(to_price(2001.00)) == 5);

    // Delete the other — level disappears
    book.update(make_del(21));
    REQUIRE(book.best_ask() == 0);
}

TEST_CASE("OrderBook: change updates qty for existing entry", "[order_book]") {
    OrderBook book;
    book.update(make_ev(30, MDUpdateAction::New,    MDEntryType::Offer, 2001.00, 10));
    book.update(make_ev(30, MDUpdateAction::Change, MDEntryType::Offer, 2001.00, 3));

    REQUIRE(book.ask_qty_at(to_price(2001.00)) == 3);
}

TEST_CASE("OrderBook: change with qty=0 removes level", "[order_book]") {
    OrderBook book;
    book.update(make_ev(40, MDUpdateAction::New,    MDEntryType::Bid, 1999.00, 10));
    book.update(make_ev(40, MDUpdateAction::Change, MDEntryType::Bid, 1999.00, 0));

    REQUIRE(book.best_bid() == 0);
    REQUIRE(book.empty());
}

TEST_CASE("OrderBook: clear resets book", "[order_book]") {
    OrderBook book;
    book.update(make_ev(1, MDUpdateAction::New, MDEntryType::Bid,   2000.00, 10));
    book.update(make_ev(2, MDUpdateAction::New, MDEntryType::Offer, 2001.00, 5));
    book.clear();

    REQUIRE(book.empty());
}

TEST_CASE("OrderBook: trade callback fires", "[order_book]") {
    OrderBook book;
    int trade_count = 0;
    book.set_trade_callback([&](const LOBEvent&) { ++trade_count; });

    book.update(make_ev(1, MDUpdateAction::New, MDEntryType::Trade, 2000.50, 2));
    REQUIRE(trade_count == 1);
}

TEST_CASE("OrderBook: cancel callback fires on delete", "[order_book]") {
    OrderBook book;
    int cancel_count = 0;
    book.set_cancel_callback([&](const LOBEvent&) { ++cancel_count; });

    book.update(make_ev(50, MDUpdateAction::New, MDEntryType::Bid, 2000.00, 5));
    book.update(make_del(50));
    REQUIRE(cancel_count == 1);
}

TEST_CASE("OrderBook: update returns true when L1 changes", "[order_book]") {
    OrderBook book;
    bool changed = book.update(
        make_ev(1, MDUpdateAction::New, MDEntryType::Bid, 2000.00, 5));
    REQUIRE(changed);

    // Adding a deeper level should not change L1
    changed = book.update(
        make_ev(2, MDUpdateAction::New, MDEntryType::Bid, 1999.00, 3));
    REQUIRE(!changed);

    // Adding a better bid changes L1
    changed = book.update(
        make_ev(3, MDUpdateAction::New, MDEntryType::Bid, 2001.00, 1));
    REQUIRE(changed);
}
