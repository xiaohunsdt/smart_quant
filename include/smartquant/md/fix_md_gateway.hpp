#pragma once

#include <atomic>
#include <chrono>
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
#include <quickfix/fix44/MarketDataRequestReject.h>
#include <quickfix/fix44/MarketDataSnapshotFullRefresh.h>
#include <quickfix/Values.h>

#include "smartquant/common/types.hpp"
#include "smartquant/common/spsc_queue.hpp"
#include "smartquant/log/lob_event_logger.hpp"
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
                 LobEventLogger& logger,
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
    // Injects cTrader credentials and TargetSubID/SenderSubID into every
    // outgoing admin message.  Reads from the [SESSION] block at startup.
    void toAdmin(FIX::Message& msg, const FIX::SessionID& sid) override;
    // Stamps TargetSubID / SenderSubID on every outgoing app message.
    void toApp(FIX::Message& msg, const FIX::SessionID& sid) override;
    // Captures Logout (35=5) reason from Tag 58 and detects fast-logout
    // (Logon accepted then immediately disconnected = auth failure).
    void fromAdmin(const FIX::Message& msg,
                   const FIX::SessionID& sid) override;

    // Returns true if the last logout looked like an authentication failure.
    [[nodiscard]] bool auth_failed() const noexcept { return auth_failed_.load(); }
    void fromApp(const FIX::Message& msg,
                 const FIX::SessionID& sid) override;

private:
    void on_incremental_refresh(
        const FIX44::MarketDataIncrementalRefresh& msg, uint64_t ts_ns);
    void on_snapshot(
        const FIX44::MarketDataSnapshotFullRefresh& msg, uint64_t ts_ns);
    void on_md_reject(const FIX44::MarketDataRequestReject& msg);

    // Send a MarketDataRequest for XAUUSD on logon
    void subscribe_market_data(const FIX::SessionID& sid);

    // Stamp TargetSubID (tag 57) and SenderSubID (tag 50) on every outgoing
    // message header.  QuickFIX 1.15.x reads these from the config for
    // session routing but does not auto-populate the wire header.
    void stamp_sub_ids(FIX::Message& msg) const;

    LobEventLogger& logger_;
    MdQueue*      queue_;
    OrderBook     book_;

    std::string target_sub_id_;
    std::string sender_sub_id_;
    // cTrader numeric symbol ID (Tag 55) and human-readable name (for logging).
    // Set via SymbolID= and SymbolName= in the [SESSION] config block.
    std::string symbol_id_;
    std::string symbol_name_{"XAUUSD"};

    std::unique_ptr<FIX::SessionSettings>  settings_;
    std::unique_ptr<FIX::FileStoreFactory> store_factory_;
    std::unique_ptr<FIX::FileLogFactory>   log_factory_;
    std::unique_ptr<FIX::SocketInitiator>  initiator_;

    FIX::SessionID session_id_;
    bool           subscribed_{false};

    // Auth-failure detection
    std::chrono::steady_clock::time_point logon_time_{};
    std::atomic<bool>                     auth_failed_{false};
};

}  // namespace sq
