#pragma once

#include <memory>
#include <string>

// QuickFIX headers
#include <quickfix/Application.h>
#include <quickfix/FileStore.h>
#include <quickfix/FileLog.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/fix44/MarketDataRequest.h>
#include <quickfix/fix44/MarketDataIncrementalRefresh.h>
#include <quickfix/fix44/MarketDataSnapshotFullRefresh.h>
#include <quickfix/Values.h>

#include "smartquant/common/types.hpp"
#include "smartquant/common/spsc_queue.hpp"
#include "smartquant/log/binary_logger.hpp"
#include "smartquant/md/order_book.hpp"

namespace sq {

// Capacity of the SPSC queue between MD gateway and Alpha threads
static constexpr std::size_t kMdQueueCapacity = 1 << 14;  // 16 384 slots

using MdQueue = SpscQueue<LOBEvent, kMdQueueCapacity>;

// FIX 4.4 Market Data Application.
//
// Receives 35=W (snapshot) and 35=X (incremental refresh) messages, updates
// the OrderBook, writes raw FIX bytes to BinaryLogger, and pushes LOBEvents
// onto the SPSC queue for the Alpha thread.
class FixMdGateway : public FIX::Application {
public:
    // `queue` may be nullptr (e.g. when running as a pure logger).
    FixMdGateway(const std::string& fix_cfg_path,
                 BinaryLogger& logger,
                 MdQueue* queue);
    ~FixMdGateway() override;

    // Start / stop the QuickFIX socket initiator
    void start();
    void stop();

    [[nodiscard]] const OrderBook& order_book() const noexcept { return book_; }
    [[nodiscard]] OrderBook& order_book()       noexcept       { return book_; }

    // ── FIX::Application interface ────────────────────────────────────────────
    void onCreate(const FIX::SessionID&) override {}
    void onLogon(const FIX::SessionID& sid) override;
    void onLogout(const FIX::SessionID& sid) override;
    // Injects cTrader credentials into the FIX Logon (35=A) message.
    // Reads Username and Password from the [SESSION] block of fix_market_data.cfg.
    void toAdmin(FIX::Message& msg, const FIX::SessionID& sid) override;
    void toApp(FIX::Message&, const FIX::SessionID&) override {}
    void fromAdmin(const FIX::Message&, const FIX::SessionID&) override {}
    void fromApp(const FIX::Message& msg,
                 const FIX::SessionID& sid) override;

private:
    void on_incremental_refresh(
        const FIX44::MarketDataIncrementalRefresh& msg, uint64_t ts_ns);
    void on_snapshot(
        const FIX44::MarketDataSnapshotFullRefresh& msg, uint64_t ts_ns);

    // Send a MarketDataRequest for XAUUSD on logon
    void subscribe_market_data(const FIX::SessionID& sid);

    BinaryLogger& logger_;
    MdQueue*      queue_;
    OrderBook     book_;

    std::unique_ptr<FIX::SessionSettings>  settings_;
    std::unique_ptr<FIX::FileStoreFactory> store_factory_;
    std::unique_ptr<FIX::FileLogFactory>   log_factory_;
    std::unique_ptr<FIX::SocketInitiator>  initiator_;

    FIX::SessionID session_id_;
    bool           subscribed_{false};
};

}  // namespace sq
