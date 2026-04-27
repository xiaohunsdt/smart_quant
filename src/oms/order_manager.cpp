#include "smartquant/oms/order_manager.hpp"

#include <cstring>   // strncpy
#include <spdlog/spdlog.h>

#include <quickfix/Session.h>
#include <quickfix/fix44/ExecutionReport.h>
#include <quickfix/fix44/OrderCancelReject.h>

#include "smartquant/common/tsc_clock.hpp"

namespace sq {

OrderManager::OrderManager(const std::string& fix_cfg_path,
                           RiskManager&       risk_mgr)
    : risk_mgr_(risk_mgr) {
    settings_      = std::make_unique<FIX::SessionSettings>(fix_cfg_path);
    store_factory_ = std::make_unique<FIX::FileStoreFactory>(*settings_);
    log_factory_   = std::make_unique<FIX::FileLogFactory>(*settings_);
    initiator_     = std::make_unique<FIX::SocketInitiator>(
        *this, *store_factory_, *settings_, *log_factory_);
}

OrderManager::~OrderManager() {
    stop();
}

void OrderManager::start() { initiator_->start(); }
void OrderManager::stop()  { if (initiator_) initiator_->stop(); }

void OrderManager::toAdmin(FIX::Message& msg, const FIX::SessionID& sid) {
    FIX::MsgType type;
    msg.getHeader().getField(type);
    if (type != FIX::MsgType_Logon) return;

    msg.setField(FIX::EncryptMethod(0));

    try {
        const FIX::Dictionary& dict = settings_->get(sid);
        if (dict.has("Username"))
            msg.setField(FIX::Username(dict.getString("Username")));
        if (dict.has("Password"))
            msg.setField(FIX::Password(dict.getString("Password")));
    } catch (const std::exception& e) {
        spdlog::error("toAdmin: failed to read credentials: {}", e.what());
    }
}

void OrderManager::onLogon(const FIX::SessionID& sid) {
    spdlog::info("FIX Trade logon: {}", sid.toString());
    session_id_ = sid;
    logged_on_.store(true, std::memory_order_release);
}

void OrderManager::onLogout(const FIX::SessionID& sid) {
    spdlog::warn("FIX Trade logout: {}", sid.toString());
    logged_on_.store(false, std::memory_order_release);
}

std::string OrderManager::next_clordid() {
    const uint64_t seq = order_seq_.fetch_add(1, std::memory_order_relaxed);
    return "SQ_" + std::to_string(seq);
}

std::string OrderManager::send_ioc(Side     side,
                                   Qty      qty,
                                   Price    signal_l1,
                                   uint64_t ts_ns) {
    if (!logged_on_.load(std::memory_order_acquire)) {
        spdlog::error("OrderManager::send_ioc: not logged on");
        return {};
    }

    const std::string clordid = next_clordid();

    // Build 35=D IOC Market order
    FIX44::NewOrderSingle order;
    order.set(FIX::ClOrdID(clordid));
    order.set(FIX::Symbol("XAUUSD"));
    order.set(FIX::Side(side == Side::Buy
                        ? FIX::Side_BUY
                        : FIX::Side_SELL));
    order.set(FIX::OrderQty(static_cast<double>(qty)));
    order.set(FIX::OrdType(FIX::OrdType_MARKET));
    order.set(FIX::TimeInForce(FIX::TimeInForce_IMMEDIATE_OR_CANCEL));
    order.set(FIX::TransactTime());  // QuickFIX fills current UTC time

    Order o{};
    std::strncpy(o.clordid, clordid.c_str(), sizeof(o.clordid) - 1);
    o.side      = side;
    o.qty       = qty;
    o.signal_l1 = signal_l1;
    o.sent_ts_ns= ts_ns;
    o.status    = OrderStatus::Sent;

    open_orders_[clordid] = o;
    risk_mgr_.on_order_sent(o, ts_ns);

    try {
        FIX::Session::sendToTarget(order, session_id_);
        spdlog::info("Sent IOC order: {} side={} qty={}",
                     clordid,
                     (side == Side::Buy ? "BUY" : "SELL"),
                     qty);
    } catch (const FIX::SessionNotFound& e) {
        spdlog::error("OrderManager::send_ioc: {}", e.what());
        open_orders_.erase(clordid);
        risk_mgr_.on_reject(o);
        return {};
    }

    return clordid;
}

void OrderManager::cancel_order(const std::string& clordid) {
    auto it = open_orders_.find(clordid);
    if (it == open_orders_.end()) return;

    const Order& o = it->second;

    FIX44::OrderCancelRequest req;
    req.set(FIX::OrigClOrdID(clordid));
    req.set(FIX::ClOrdID(next_clordid()));
    req.set(FIX::Symbol("XAUUSD"));
    req.set(FIX::Side(o.side == Side::Buy ? FIX::Side_BUY : FIX::Side_SELL));
    req.set(FIX::TransactTime());

    try {
        FIX::Session::sendToTarget(req, session_id_);
    } catch (const FIX::SessionNotFound& e) {
        spdlog::error("OrderManager::cancel_order: {}", e.what());
    }
}

void OrderManager::fromApp(const FIX::Message&   msg,
                           const FIX::SessionID& /*sid*/) {
    FIX::MsgType msg_type;
    msg.getHeader().getField(msg_type);

    try {
        if (msg_type == FIX::MsgType_ExecutionReport) {
            on_execution_report(static_cast<const FIX44::ExecutionReport&>(msg));
        } else if (msg_type == FIX::MsgType_OrderCancelReject) {
            on_cancel_reject(static_cast<const FIX44::OrderCancelReject&>(msg));
        }
    } catch (const FIX::FieldNotFound& e) {
        spdlog::warn("OrderManager: field not found: {}", e.what());
    }
}

void OrderManager::on_execution_report(const FIX44::ExecutionReport& msg) {
    FIX::ClOrdID  clordid;
    FIX::ExecType exec_type;
    msg.get(clordid);
    msg.get(exec_type);

    const std::string id = clordid.getValue();
    auto it = open_orders_.find(id);
    if (it == open_orders_.end()) return;

    Order& o = it->second;

    const char et = exec_type.getValue();

    if (et == FIX::ExecType_FILL || et == FIX::ExecType_PARTIAL_FILL) {
        FIX::LastPx last_px;
        FIX::LastQty last_qty;
        if (msg.isSetField(last_px.getField())) msg.get(last_px);
        if (msg.isSetField(last_qty.getField())) msg.get(last_qty);

        o.fill_price  = to_price(last_px.getValue());
        o.fill_ts_ns  = TscClock::now_ns();
        o.status      = (et == FIX::ExecType_FILL) ? OrderStatus::Filled : OrderStatus::Pending;  // partial

        const uint64_t latency_ns = o.fill_ts_ns - o.sent_ts_ns;
        spdlog::info("Fill: {} price={:.2f} latency={}µs", id, from_price(o.fill_price), latency_ns / 1000);

        risk_mgr_.on_fill(o, o.fill_ts_ns);
        if (on_fill_) on_fill_(o);

        if (et == FIX::ExecType_FILL)
            open_orders_.erase(it);

    } else if (et == FIX::ExecType_REJECTED || et == FIX::ExecType_EXPIRED) {
        o.status = (et == FIX::ExecType_REJECTED) ? OrderStatus::Rejected : OrderStatus::Cancelled;
        spdlog::warn("Order {} {}", id, (et == FIX::ExecType_REJECTED) ? "rejected" : "expired");
        risk_mgr_.on_reject(o);
        if (on_reject_) on_reject_(o);
        open_orders_.erase(it);
    }
}

void OrderManager::on_cancel_reject(const FIX44::OrderCancelReject& msg) {
    FIX::ClOrdID clordid;
    msg.get(clordid);
    spdlog::warn("CancelReject for order: {}", clordid.getValue());
}

}  // namespace sq
