#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "smartquant/md/order_book.hpp"

using namespace sq;

static LOBEvent make_ev(MDUpdateAction act, MDEntryType side,
                        double price, Qty qty,
                        uint64_t ts = 1000) {
    return LOBEvent{ts, to_price(price), qty, act, side};
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
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Bid,   2000.50, 10));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Offer, 2000.60, 5));

    REQUIRE(book.best_bid() == to_price(2000.50));
    REQUIRE(book.best_ask() == to_price(2000.60));
    REQUIRE(book.spread()   == to_price(2000.60) - to_price(2000.50));
    REQUIRE(!book.empty());
}

TEST_CASE("OrderBook: best bid is highest bid price", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Bid, 1990.00, 5));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Bid, 1995.00, 3));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Bid, 1993.00, 8));

    REQUIRE(book.best_bid() == to_price(1995.00));
}

TEST_CASE("OrderBook: best ask is lowest ask price", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Offer, 2005.00, 5));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Offer, 2001.00, 3));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Offer, 2003.00, 8));

    REQUIRE(book.best_ask() == to_price(2001.00));
}

TEST_CASE("OrderBook: delete removes level", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New,    MDEntryType::Bid, 2000.00, 10));
    book.update(make_ev(MDUpdateAction::New,    MDEntryType::Bid, 1999.00, 5));
    book.update(make_ev(MDUpdateAction::Delete, MDEntryType::Bid, 2000.00, 0));

    REQUIRE(book.best_bid() == to_price(1999.00));
    REQUIRE(book.bid_depth() == 1);
}

TEST_CASE("OrderBook: change updates qty", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New,    MDEntryType::Offer, 2001.00, 10));
    book.update(make_ev(MDUpdateAction::Change, MDEntryType::Offer, 2001.00, 3));

    REQUIRE(book.ask_qty_at(to_price(2001.00)) == 3);
}

TEST_CASE("OrderBook: change with qty=0 removes level", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New,    MDEntryType::Bid, 1999.00, 10));
    book.update(make_ev(MDUpdateAction::Change, MDEntryType::Bid, 1999.00, 0));

    REQUIRE(book.best_bid() == 0);
    REQUIRE(book.empty());
}

TEST_CASE("OrderBook: clear resets book", "[order_book]") {
    OrderBook book;
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Bid,   2000.00, 10));
    book.update(make_ev(MDUpdateAction::New, MDEntryType::Offer, 2001.00, 5));
    book.clear();

    REQUIRE(book.empty());
}

TEST_CASE("OrderBook: trade callback fires", "[order_book]") {
    OrderBook book;
    int trade_count = 0;
    book.set_trade_callback([&](const LOBEvent&) { ++trade_count; });

    book.update(make_ev(MDUpdateAction::New, MDEntryType::Trade, 2000.50, 2));
    REQUIRE(trade_count == 1);
}

TEST_CASE("OrderBook: cancel callback fires on delete", "[order_book]") {
    OrderBook book;
    int cancel_count = 0;
    book.set_cancel_callback([&](const LOBEvent&) { ++cancel_count; });

    book.update(make_ev(MDUpdateAction::New,    MDEntryType::Bid, 2000.00, 5));
    book.update(make_ev(MDUpdateAction::Delete, MDEntryType::Bid, 2000.00, 0));
    REQUIRE(cancel_count == 1);
}

TEST_CASE("OrderBook: update returns true when L1 changes", "[order_book]") {
    OrderBook book;
    bool changed = book.update(
        make_ev(MDUpdateAction::New, MDEntryType::Bid, 2000.00, 5));
    REQUIRE(changed);

    // Adding a deeper level should not change L1
    changed = book.update(
        make_ev(MDUpdateAction::New, MDEntryType::Bid, 1999.00, 3));
    REQUIRE(!changed);

    // Adding a better bid changes L1
    changed = book.update(
        make_ev(MDUpdateAction::New, MDEntryType::Bid, 2001.00, 1));
    REQUIRE(changed);
}
