#include "smartquant/md/order_book.hpp"

namespace sq {

bool OrderBook::update(const LOBEvent& ev) {
    const Price prev_bid = best_bid();
    const Price prev_ask = best_ask();

    // Helper: add qty to a price level, removing the level when qty reaches 0.
    auto level_add = [](std::map<Price, Qty>& book, Price px, Qty delta) {
        auto& slot = book[px];
        slot += delta;
        if (slot <= 0) book.erase(px);
    };

    switch (ev.action) {
    case MDUpdateAction::New: {
        if (ev.qty <= 0) break;
        entries_[ev.entry_id] = {ev.side, ev.price, ev.qty};
        if (ev.side == MDEntryType::Bid)
            level_add(bids_, ev.price, ev.qty);
        else if (ev.side == MDEntryType::Offer)
            level_add(asks_, ev.price, ev.qty);
        else if (on_trade_) on_trade_(ev);
        break;
    }
    case MDUpdateAction::Change: {
        // Remove old contribution, apply new.
        auto it = entries_.find(ev.entry_id);
        if (it != entries_.end()) {
            const EntryInfo& old = it->second;
            if (old.side == MDEntryType::Bid)
                level_add(bids_, old.price, -old.qty);
            else if (old.side == MDEntryType::Offer)
                level_add(asks_, old.price, -old.qty);
        }
        if (ev.qty > 0) {
            entries_[ev.entry_id] = {ev.side, ev.price, ev.qty};
            if (ev.side == MDEntryType::Bid)
                level_add(bids_, ev.price, ev.qty);
            else if (ev.side == MDEntryType::Offer)
                level_add(asks_, ev.price, ev.qty);
        } else {
            entries_.erase(ev.entry_id);
        }
        break;
    }
    case MDUpdateAction::Delete: {
        // cTrader sends Delete with entry_id only — no price or side.
        auto it = entries_.find(ev.entry_id);
        if (it != entries_.end()) {
            const EntryInfo& old = it->second;
            LOBEvent cancel_ev  = ev;
            cancel_ev.side      = old.side;
            cancel_ev.price     = old.price;
            cancel_ev.qty       = old.qty;
            if (old.side == MDEntryType::Bid)
                level_add(bids_, old.price, -old.qty);
            else if (old.side == MDEntryType::Offer)
                level_add(asks_, old.price, -old.qty);
            entries_.erase(it);
            if (on_cancel_) on_cancel_(cancel_ev);
        }
        break;
    }
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

bool OrderBook::lookup_entry(uint64_t entry_id,
                             MDEntryType& side,
                             Price&       price,
                             Qty&         qty) const noexcept {
    auto it = entries_.find(entry_id);
    if (it == entries_.end()) return false;
    side  = it->second.side;
    price = it->second.price;
    qty   = it->second.qty;
    return true;
}

void OrderBook::clear() noexcept {
    bids_.clear();
    asks_.clear();
    entries_.clear();
}

}  // namespace sq
