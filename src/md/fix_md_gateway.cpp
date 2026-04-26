#include "smartquant/md/fix_md_gateway.hpp"

#include <spdlog/spdlog.h>

#include <quickfix/Session.h>
#include <quickfix/Values.h>
#include <quickfix/fix44/MarketDataRequest.h>

#include "smartquant/common/tsc_clock.hpp"

namespace sq {

FixMdGateway::FixMdGateway(const std::string& fix_cfg_path,
                            BinaryLogger&      logger,
                            MdQueue*           queue)
    : logger_(logger), queue_(queue) {
    settings_      = std::make_unique<FIX::SessionSettings>(fix_cfg_path);
    store_factory_ = std::make_unique<FIX::FileStoreFactory>(*settings_);
    log_factory_   = std::make_unique<FIX::FileLogFactory>(*settings_);
    initiator_     = std::make_unique<FIX::SocketInitiator>(
        *this, *store_factory_, *settings_, *log_factory_);

    // Cache SubID values from the first session block so we can stamp them on
    // every outgoing message.  QuickFIX 1.15.x reads these for session routing
    // but does not auto-populate tag 50/57 on the wire.
    try {
        const auto sessions = settings_->getSessions();
        if (!sessions.empty()) {
            const FIX::Dictionary& dict = settings_->get(*sessions.begin());
            if (dict.has("TargetSubID"))
                target_sub_id_ = dict.getString("TargetSubID");
            if (dict.has("SenderSubID"))
                sender_sub_id_ = dict.getString("SenderSubID");
        }
    } catch (const std::exception& e) {
        spdlog::warn("FixMdGateway: could not read SubID settings: {}", e.what());
    }

    if (!target_sub_id_.empty())
        spdlog::info("FIX MD TargetSubID = {}", target_sub_id_);
}

FixMdGateway::~FixMdGateway() {
    stop();
}

void FixMdGateway::start() {
    initiator_->start();
}

void FixMdGateway::stop() {
    if (initiator_) initiator_->stop();
}

void FixMdGateway::stamp_sub_ids(FIX::Message& msg) const {
    if (!target_sub_id_.empty())
        msg.getHeader().setField(FIX::TargetSubID(target_sub_id_));
    if (!sender_sub_id_.empty())
        msg.getHeader().setField(FIX::SenderSubID(sender_sub_id_));
}

void FixMdGateway::toAdmin(FIX::Message& msg, const FIX::SessionID& sid) {
    // Always stamp SubIDs — cTrader validates tag 57 on every admin message.
    stamp_sub_ids(msg);

    FIX::MsgType type;
    msg.getHeader().getField(type);
    if (type != FIX::MsgType_Logon) return;

    // cTrader requires EncryptMethod=0, Username (Tag 553), Password (Tag 554)
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

void FixMdGateway::toApp(FIX::Message& msg, const FIX::SessionID& /*sid*/) {
    stamp_sub_ids(msg);
}

void FixMdGateway::onLogon(const FIX::SessionID& sid) {
    spdlog::info("FIX MD logon OK: {}", sid.toString());
    session_id_ = sid;
    auth_failed_.store(false);
    logon_time_ = std::chrono::steady_clock::now();
    subscribe_market_data(sid);
}

void FixMdGateway::onLogout(const FIX::SessionID& sid) {
    // Detect fast-logout: if we were disconnected within 3 s of logon,
    // it almost certainly means the server rejected our credentials.
    if (logon_time_.time_since_epoch().count() != 0) {
        const auto alive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - logon_time_).count();
        if (alive_ms < 3000) {
            auth_failed_.store(true);
            spdlog::error(
                "FIX MD auth failure — session lived only {}ms. "
                "Check SenderCompID / Username / Password in fix_market_data.cfg",
                alive_ms);
            return;
        }
    }
    spdlog::warn("FIX MD logout: {}", sid.toString());
    subscribed_ = false;
}

void FixMdGateway::fromAdmin(const FIX::Message&  msg, const FIX::SessionID& /*sid*/) {
    FIX::MsgType msg_type;
    msg.getHeader().getField(msg_type);

    // Only interested in Logout (35=5) — it may carry a rejection reason
    if (msg_type.getValue() != FIX::MsgType_Logout) return;

    FIX::Text text;
    if (msg.isSetField(text)) {
        msg.getField(text);
        const std::string& reason = text.getValue();
        spdlog::error("FIX MD logout reason (Tag 58): \"{}\"", reason);

        // Common cTrader rejection strings
        const bool is_auth =
            reason.find("Invalid") != std::string::npos ||
            reason.find("credential") != std::string::npos ||
            reason.find("Unauthori") != std::string::npos ||
            reason.find("password") != std::string::npos ||
            reason.find("Password") != std::string::npos ||
            reason.find("Username") != std::string::npos ||
            reason.find("Login") != std::string::npos;

        const bool is_subid =
            reason.find("TargetSubID") != std::string::npos ||
            reason.find("SenderSubID") != std::string::npos;

        if (is_auth) {
            auth_failed_.store(true);
            spdlog::error(
                "==> Authentication failure detected. "
                "Verify SenderCompID / Username / Password in fix_market_data.cfg");
        } else if (is_subid) {
            auth_failed_.store(true);
            spdlog::error(
                "==> Session identity mismatch. "
                "Verify TargetSubID (expected 'QUOTE') in fix_market_data.cfg");
        }
    }
}

void FixMdGateway::fromApp(const FIX::Message&   msg,
                           const FIX::SessionID& sid) {
    // TSC timestamp is captured as the FIRST operation — before any parsing
    const uint64_t ts_ns = TscClock::now_ns();

    // Write raw FIX bytes to binary log
    const std::string raw = msg.toString();
    logger_.write(ts_ns, raw.data(), static_cast<uint32_t>(raw.size()));

    const FIX::MsgType msg_type;
    msg.getHeader().getField(const_cast<FIX::MsgType&>(msg_type));
    const std::string& type = msg_type.getValue();

    try {
        if (type == "X") {
            on_incremental_refresh(
                static_cast<const FIX44::MarketDataIncrementalRefresh&>(msg),
                ts_ns);
        } else if (type == "W") {
            on_snapshot(
                static_cast<const FIX44::MarketDataSnapshotFullRefresh&>(msg),
                ts_ns);
        }
    } catch (const FIX::FieldNotFound& e) {
        spdlog::warn("FIX field not found in {}: {}", type, e.what());
    }

    (void)sid;
}

void FixMdGateway::on_incremental_refresh(
    const FIX44::MarketDataIncrementalRefresh& msg, uint64_t ts_ns) {

    FIX::NoMDEntries num_entries;
    msg.get(num_entries);

    FIX44::MarketDataIncrementalRefresh::NoMDEntries group;

    for (int i = 1; i <= num_entries; ++i) {
        msg.getGroup(i, group);

        FIX::MDUpdateAction action_field;
        FIX::MDEntryType    type_field;
        FIX::MDEntryPx      px_field;
        FIX::MDEntrySize    sz_field;

        group.get(action_field);
        group.get(type_field);

        LOBEvent ev{};
        ev.timestamp_ns = ts_ns;

        // MDUpdateAction: '0'=New, '1'=Change, '2'=Delete
        switch (action_field.getValue()) {
        case '0': ev.action = MDUpdateAction::New;    break;
        case '1': ev.action = MDUpdateAction::Change; break;
        case '2': ev.action = MDUpdateAction::Delete; break;
        default:  continue;
        }

        // MDEntryType: '0'=Bid, '1'=Offer, '2'=Trade
        switch (type_field.getValue()) {
        case '0': ev.side = MDEntryType::Bid;   break;
        case '1': ev.side = MDEntryType::Offer; break;
        case '2': ev.side = MDEntryType::Trade; break;
        default:  continue;
        }

        if (group.isSetField(px_field.getField())) {
            group.get(px_field);
            ev.price = to_price(px_field.getValue());
        }
        if (group.isSetField(sz_field.getField())) {
            group.get(sz_field);
            ev.qty = static_cast<Qty>(sz_field.getValue());
        }

        // Update in-process order book
        book_.update(ev);

        // Push to Alpha thread (best-effort — if queue full, drop)
        if (queue_) {
            if (!queue_->push(ev))
                spdlog::warn("MdQueue full — dropped LOBEvent");
        }
    }
}

void FixMdGateway::on_snapshot(
    const FIX44::MarketDataSnapshotFullRefresh& msg, uint64_t ts_ns) {

    // Full snapshot: clear existing book and rebuild from scratch
    book_.clear();

    FIX::NoMDEntries num_entries;
    msg.get(num_entries);

    FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;

    for (int i = 1; i <= num_entries; ++i) {
        msg.getGroup(i, group);

        FIX::MDEntryType type_field;
        FIX::MDEntryPx   px_field;
        FIX::MDEntrySize sz_field;

        group.get(type_field);

        LOBEvent ev{};
        ev.timestamp_ns = ts_ns;
        ev.action       = MDUpdateAction::New;

        switch (type_field.getValue()) {
        case '0': ev.side = MDEntryType::Bid;   break;
        case '1': ev.side = MDEntryType::Offer; break;
        default:  continue;  // Skip trade entries in snapshot
        }

        if (group.isSetField(px_field.getField())) {
            group.get(px_field);
            ev.price = to_price(px_field.getValue());
        }
        if (group.isSetField(sz_field.getField())) {
            group.get(sz_field);
            ev.qty = static_cast<Qty>(sz_field.getValue());
        }

        book_.update(ev);
        if (queue_) queue_->push(ev);
    }

    spdlog::info("LOB snapshot applied: bid={:.2f} ask={:.2f}",
                 from_price(book_.best_bid()),
                 from_price(book_.best_ask()));
}

void FixMdGateway::subscribe_market_data(const FIX::SessionID& sid) {
    if (subscribed_) return;

    FIX44::MarketDataRequest req;
    req.set(FIX::MDReqID("XAUUSD_SUB_1"));
    req.set(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES));
    req.set(FIX::MarketDepth(0));   // 0 = full book
    req.set(FIX::MDUpdateType(FIX::MDUpdateType_INCREMENTAL_REFRESH));

    // Entry types: Bid, Offer, Trade
    FIX44::MarketDataRequest::NoMDEntryTypes type_group;
    type_group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
    req.addGroup(type_group);
    type_group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
    req.addGroup(type_group);
    type_group.set(FIX::MDEntryType(FIX::MDEntryType_TRADE));
    req.addGroup(type_group);

    // Symbol: XAUUSD
    FIX44::MarketDataRequest::NoRelatedSym sym_group;
    sym_group.set(FIX::Symbol("XAUUSD"));
    req.addGroup(sym_group);

    FIX::Session::sendToTarget(req, sid);
    subscribed_ = true;
    spdlog::info("Sent MarketDataRequest for XAUUSD");
}

}  // namespace sq
