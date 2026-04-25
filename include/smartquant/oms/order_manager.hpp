#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <quickfix/Application.h>
#include <quickfix/FileStore.h>
#include <quickfix/FileLog.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/fix44/NewOrderSingle.h>
#include <quickfix/fix44/OrderCancelRequest.h>
#include <quickfix/fix44/ExecutionReport.h>
#include <quickfix/fix44/OrderCancelReject.h>
#include <quickfix/Values.h>

#include "smartquant/common/types.hpp"
#include "smartquant/risk/risk_manager.hpp"

namespace sq {

// FIX 4.4 Order Management Application.
//
// Manages the trade FIX session and provides send_ioc() for firing IOC market
// orders and cancel_order() for explicit cancellations.
//
// All execution reports are routed to the supplied RiskManager via callbacks.
class OrderManager : public FIX::Application {
public:
    using FillCallback   = std::function<void(const Order&)>;
    using RejectCallback = std::function<void(const Order&)>;

    OrderManager(const std::string& fix_cfg_path,
                 RiskManager&       risk_mgr);
    ~OrderManager() override;

    void start();
    void stop();

    // Send an IOC market order.  Returns the ClOrdID or empty string on error.
    std::string send_ioc(Side side, Qty qty, Price signal_l1, uint64_t ts_ns);

    // Cancel an open order by ClOrdID.
    void cancel_order(const std::string& clordid);

    // Optional callbacks (called from the FIX thread)
    void set_fill_callback(FillCallback cb)     { on_fill_   = std::move(cb); }
    void set_reject_callback(RejectCallback cb) { on_reject_ = std::move(cb); }

    [[nodiscard]] bool is_logged_on() const noexcept { return logged_on_; }

    // ── FIX::Application interface ────────────────────────────────────────────
    void onCreate(const FIX::SessionID&) override {}
    void onLogon(const FIX::SessionID& sid) override;
    void onLogout(const FIX::SessionID& sid) override;
    // Injects cTrader credentials into FIX Logon (35=A).
    // Reads Username and Password from the [SESSION] block of fix_trade.cfg.
    void toAdmin(FIX::Message& msg, const FIX::SessionID& sid) override;
    void toApp(FIX::Message&, const FIX::SessionID&) override {}
    void fromAdmin(const FIX::Message&, const FIX::SessionID&) override {}
    void fromApp(const FIX::Message& msg, const FIX::SessionID& sid) override;

private:
    void on_execution_report(const FIX44::ExecutionReport& msg);
    void on_cancel_reject(const FIX44::OrderCancelReject& msg);

    std::string next_clordid();

    RiskManager& risk_mgr_;

    std::unique_ptr<FIX::SessionSettings>  settings_;
    std::unique_ptr<FIX::FileStoreFactory> store_factory_;
    std::unique_ptr<FIX::FileLogFactory>   log_factory_;
    std::unique_ptr<FIX::SocketInitiator>  initiator_;

    FIX::SessionID session_id_;
    std::atomic<bool> logged_on_{false};

    std::atomic<uint64_t> order_seq_{1};

    // Open orders indexed by ClOrdID
    std::unordered_map<std::string, Order> open_orders_;

    FillCallback   on_fill_;
    RejectCallback on_reject_;
};

}  // namespace sq
