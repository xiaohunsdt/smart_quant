#pragma once

#include <cstdint>
#include <map>
#include <functional>

#include "smartquant/common/types.hpp"

namespace sq {

// L1/L2 limit order book for a single instrument.
//
// Prices are stored as integer ticks (Price = int64_t, scaled by kPriceScale).
// The book maintains sorted bid/ask maps and computes best-bid/best-ask in O(1)
// via rbegin()/begin() on the respective std::map.
class OrderBook {
public:
    // Callback types for derived data consumers
    using TradeCallback  = std::function<void(const LOBEvent&)>;
    using CancelCallback = std::function<void(const LOBEvent&)>;

    OrderBook() = default;

    // Apply one LOB event from the FIX 35=X stream.
    // Returns true if the L1 (best bid/ask) changed after this update.
    bool update(const LOBEvent& ev);

    // ── Queries ────────────────────────────────────────────────────────────────
    [[nodiscard]] Price best_bid()   const noexcept;  // 0 if empty
    [[nodiscard]] Price best_ask()   const noexcept;  // 0 if empty
    [[nodiscard]] Price spread()     const noexcept;  // best_ask - best_bid
    [[nodiscard]] Qty   bid_qty_at(Price p) const noexcept;
    [[nodiscard]] Qty   ask_qty_at(Price p) const noexcept;
    [[nodiscard]] bool  empty()      const noexcept;

    // ── Callbacks (optional) ───────────────────────────────────────────────────
    void set_trade_callback(TradeCallback cb)  { on_trade_  = std::move(cb); }
    void set_cancel_callback(CancelCallback cb){ on_cancel_ = std::move(cb); }

    // Reset the book (e.g. on session reconnect)
    void clear() noexcept;

    // Depth on each side (number of price levels)
    [[nodiscard]] std::size_t bid_depth() const noexcept { return bids_.size(); }
    [[nodiscard]] std::size_t ask_depth() const noexcept { return asks_.size(); }

private:
    // bids: highest price first → use default map (ascending) + rbegin
    // asks: lowest price first  → use default map (ascending) + begin
    std::map<Price, Qty> bids_;   // price → qty
    std::map<Price, Qty> asks_;   // price → qty

    TradeCallback  on_trade_;
    CancelCallback on_cancel_;
};

}  // namespace sq
