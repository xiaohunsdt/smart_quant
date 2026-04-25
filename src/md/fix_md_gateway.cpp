#include "smartquant/md/fix_md_gateway.hpp"

#include <spdlog/spdlog.h>

#include <quickfix/Session.h>
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

void FixMdGateway::toAdmin(FIX::Message& msg, const FIX::SessionID& sid) {
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

void FixMdGateway::onLogon(const FIX::SessionID& sid) {
    spdlog::info("FIX MD logon: {}", sid.toString());
    session_id_ = sid;
    subscribe_market_data(sid);
}

void FixMdGateway::onLogout(const FIX::SessionID& sid) {
    spdlog::warn("FIX MD logout: {}", sid.toString());
    subscribed_ = false;
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
