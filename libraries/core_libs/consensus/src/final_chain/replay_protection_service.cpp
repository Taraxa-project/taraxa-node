
#include "final_chain/replay_protection_service.hpp"

#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>

#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "common/util.hpp"

namespace taraxa::final_chain {
using namespace dev;
using namespace util;
using namespace std;

string senderStateKey(string const& sender_addr_hex) { return "sender_" + sender_addr_hex; }

string periodDataKeysKey(uint64_t period) { return "data_keys_at_" + to_string(period); }

string maxNonceAtRoundKey(uint64_t period, string const& sender_addr_hex) {
  return "max_nonce_at_" + to_string(period) + "_" + sender_addr_hex;
}

struct SenderState {
  trx_nonce_t nonce_max = 0;
  optional<trx_nonce_t> nonce_watermark;

  explicit SenderState(const trx_nonce_t& nonce_max) : nonce_max(nonce_max) {}
  explicit SenderState(const RLP& rlp)
      : nonce_max(rlp[0].toInt<trx_nonce_t>()),
        nonce_watermark(rlp[1].toInt<bool>() ? optional(rlp[2].toInt<trx_nonce_t>()) : std::nullopt) {}

  bytes rlp() {
    RLPStream rlp(3);
    rlp << nonce_max;
    rlp << nonce_watermark.has_value();
    rlp << (nonce_watermark ? *nonce_watermark : 0);
    return rlp.invalidate();
  }
};

DB::Slice db_slice(dev::bytes const& b) { return {(char*)b.data(), b.size()}; }

class ReplayProtectionServiceImpl : public virtual ReplayProtectionService {
  Config config_;
  shared_ptr<DB> db_;
  shared_mutex mutable mu_;

 public:
  ReplayProtectionServiceImpl(Config const& config, shared_ptr<DB> db) : config_(config), db_(move(db)) {}

  bool is_nonce_stale(const addr_t& addr, const trx_nonce_t& nonce) const override {
    shared_lock l(mu_);
    auto sender_state = loadSenderState(senderStateKey(addr.hex()));
    if (!sender_state) {
      return false;
    }
    return sender_state->nonce_watermark && nonce <= *sender_state->nonce_watermark;
  }

  // TODO use binary types instead of hex strings
  void update(DB::Batch& batch, uint64_t period, RangeView<TransactionInfo> const& trxs) override {
    unique_lock l(mu_);
    unordered_map<string, shared_ptr<SenderState>> sender_states;
    sender_states.reserve(trxs.size);
    unordered_map<string, shared_ptr<SenderState>> sender_states_dirty;
    sender_states_dirty.reserve(trxs.size);
    trxs.for_each([&, this](auto const& trx) {
      auto sender_addr = trx.sender.hex();
      shared_ptr<SenderState> sender_state;
      if (auto e = sender_states.find(sender_addr); e != sender_states.end()) {
        sender_state = e->second;
      } else {
        sender_state = loadSenderState(senderStateKey(sender_addr));
        if (!sender_state) {
          sender_states[sender_addr] = sender_states_dirty[sender_addr] = std::make_shared<SenderState>(trx.nonce);
          return;
        }
        sender_states[sender_addr] = sender_state;
      }
      if (sender_state->nonce_max < trx.nonce) {
        sender_state->nonce_max = trx.nonce;
        sender_states_dirty[sender_addr] = sender_state;
      }
    });
    stringstream period_data_keys;
    for (auto const& [sender, state] : sender_states_dirty) {
      db_->insert(batch, DB::Columns::final_chain_replay_protection, maxNonceAtRoundKey(period, sender),
                  toString(state->nonce_max));
      db_->insert(batch, DB::Columns::final_chain_replay_protection, senderStateKey(sender), db_slice(state->rlp()));
      period_data_keys << sender << "\n";
    }
    if (auto v = period_data_keys.str(); !v.empty()) {
      db_->insert(batch, DB::Columns::final_chain_replay_protection, periodDataKeysKey(period), v);
    }
    if (period < config_.range) {
      return;
    }
    auto bottom_period = period - config_.range;
    auto bottom_period_data_keys_key = periodDataKeysKey(bottom_period);
    auto keys = db_->lookup(bottom_period_data_keys_key, DB::Columns::final_chain_replay_protection);
    if (keys.empty()) {
      return;
    }
    istringstream is(keys);
    for (string line; getline(is, line);) {
      auto nonce_max_key = maxNonceAtRoundKey(bottom_period, line);
      if (auto v = db_->lookup(nonce_max_key, DB::Columns::final_chain_replay_protection); !v.empty()) {
        auto sender_state_key = senderStateKey(line);
        auto state = loadSenderState(sender_state_key);
        state->nonce_watermark = stoull(v);
        db_->insert(batch, DB::Columns::final_chain_replay_protection, sender_state_key, db_slice(state->rlp()));
        db_->remove(batch, DB::Columns::final_chain_replay_protection, nonce_max_key);
      }
    }
    db_->remove(batch, DB::Columns::final_chain_replay_protection, bottom_period_data_keys_key);
  }

  shared_ptr<SenderState> loadSenderState(string const& key) const {
    if (auto v = db_->lookup(key, DB::Columns::final_chain_replay_protection); !v.empty()) {
      return std::make_shared<SenderState>(RLP(v));
    }
    return nullptr;
  }
};

// Only for tests
std::unique_ptr<ReplayProtectionService> NewReplayProtectionService(ReplayProtectionService::Config config,
                                                                    std::shared_ptr<DB> db) {
  return make_unique<ReplayProtectionServiceImpl>(config, move(db));
}

Json::Value enc_json(ReplayProtectionService::Config const& obj) {
  Json::Value json(Json::objectValue);
  json["range"] = dev::toJS(obj.range);
  return json;
}

void dec_json(Json::Value const& json, ReplayProtectionService::Config& obj) {
  obj.range = dev::jsToInt(json["range"].asString());
}

}  // namespace taraxa::final_chain