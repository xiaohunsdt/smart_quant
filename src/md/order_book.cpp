#include "smartquant/md/order_book.hpp"

namespace sq {

bool OrderBook::update(const LOBEvent& ev) {
    const Price prev_bid = best_bid();
    const Price prev_ask = best_ask();

    auto apply = [&](std::map<Price, Qty>& side) {
        switch (ev.action) {
        case MDUpdateAction::New:
        case MDUpdateAction::Change:
            if (ev.qty <= 0)
                side.erase(ev.price);
            else
                side[ev.price] = ev.qty;
            break;
        case MDUpdateAction::Delete:
            side.erase(ev.price);
            if (on_cancel_) on_cancel_(ev);
            break;
        }
    };

    switch (ev.side) {
    case MDEntryType::Bid:
        apply(bids_);
        break;
    case MDEntryType::Offer:
        apply(asks_);
        break;
    case MDEntryType::Trade:
        if (on_trade_) on_trade_(ev);
        break;
    }

    return (best_bid() != prev_bid) || (best_ask() != prev_ask);
}

Price OrderBook::best_bid() const noexcept {
    return bids_.empty() ? 0 : bids_.rbegin()->first;
}

Price OrderBook::best_ask() const noexcept {
    return asks_.empty() ? 0 : asks_.begin()->first;
}

Price OrderBook::spread() const noexcept {
    const Price b = best_bid();
    const Price a = best_ask();
    return (b > 0 && a > 0) ? (a - b) : 0;
}

Qty OrderBook::bid_qty_at(Price p) const noexcept {
    auto it = bids_.find(p);
    return (it != bids_.end()) ? it->second : 0;
}

Qty OrderBook::ask_qty_at(Price p) const noexcept {
    auto it = asks_.find(p);
    return (it != asks_.end()) ? it->second : 0;
}

bool OrderBook::empty() const noexcept {
    return bids_.empty() && asks_.empty();
}

void OrderBook::clear() noexcept {
    bids_.clear();
    asks_.clear();
}

}  // namespace sq
