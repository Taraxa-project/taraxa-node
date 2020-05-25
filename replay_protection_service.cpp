#include "replay_protection_service.hpp"

#include <libdevcore/RLP.h>

#include <sstream>
#include <unordered_map>

#include "util.hpp"

namespace taraxa::replay_protection_service {
using dev::fromBigEndian;
using dev::RLP;
using dev::toBigEndianString;
using std::getline;
using std::istringstream;
using std::make_shared;
using std::move;
using std::nullopt;
using std::shared_lock;
using std::stringstream;
using std::to_string;
using std::unique_lock;
using std::unordered_map;

string senderStateKey(string const& sender_addr_hex) {
  return "sender_" + sender_addr_hex;
}

string roundDataKeysKey(round_t round) {
  return "data_keys_at_" + to_string(round);
}

string trxHashKey(string const& trx_hash_hex) { return "trx_" + trx_hash_hex; }

string maxNonceAtRoundKey(round_t round, string const& sender_addr_hex) {
  return "max_nonce_at_" + to_string(round) + "_" + sender_addr_hex;
}

struct SenderState {
  uint64_t nonce_max = 0;
  optional<uint64_t> nonce_watermark;

  SenderState(uint64_t nonce_max) : nonce_max(nonce_max) {}
  explicit SenderState(RLP const& rlp)
      : nonce_max(rlp[0].toInt<trx_nonce_t>()),
        nonce_watermark(rlp[1].toInt<bool>()
                            ? optional(rlp[2].toInt<uint64_t>())
                            : std::nullopt) {}

  bytes rlp() {
    RLPStream rlp(3);
    rlp << nonce_max;
    rlp << nonce_watermark.has_value();
    rlp << (nonce_watermark ? *nonce_watermark : 0);
    return rlp.out();
  }
};

shared_ptr<SenderState> loadSenderState(shared_ptr<DbStorage> db_,
                                        string const& key) {
  if (auto v = db_->lookup(key, DbStorage::Columns::replay_protection);
      !v.empty()) {
    return s_ptr(new SenderState(RLP(v)));
  }
  return nullptr;
}

rocksdb::Slice db_slice(dev::bytes const& b) {
  return {(char*)b.data(), b.size()};
}

ReplayProtectionService::ReplayProtectionService(
    decltype(range_) range, shared_ptr<DbStorage> const& db_storage)
    : range_(range), db_(db_storage) {
  assert(range_ > 0);
}

bool ReplayProtectionService::is_nonce_stale(addr_t const& addr,
                                             uint64_t nonce) {
  shared_lock l(m_);
  auto sender_state = loadSenderState(db_, senderStateKey(addr.hex()));
  if (!sender_state) {
    return false;
  }
  return sender_state->nonce_watermark &&
         nonce <= *sender_state->nonce_watermark;
}

// TODO use binary types instead of hex strings
void ReplayProtectionService::register_executed_transactions(
    DbStorage::BatchPtr batch, round_t round,
    RangeView<TransactionInfo> const& trxs) {
  unique_lock l(m_);
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
      sender_state = loadSenderState(db_, senderStateKey(sender_addr));
      if (!sender_state) {
        sender_states[sender_addr] = sender_states_dirty[sender_addr] =
            s_ptr(new SenderState(trx.nonce));
        return;
      }
      sender_states[sender_addr] = sender_state;
    }
    if (sender_state->nonce_max < trx.nonce) {
      sender_state->nonce_max = trx.nonce;
      sender_states_dirty[sender_addr] = sender_state;
    }
  });
  stringstream round_data_keys;
  for (auto const& [sender, state] : sender_states_dirty) {
    db_->batch_put(batch, DbStorage::Columns::replay_protection,
                   maxNonceAtRoundKey(round, sender),
                   to_string(state->nonce_max));
    db_->batch_put(batch, DbStorage::Columns::replay_protection,
                   senderStateKey(sender), db_slice(state->rlp()));
    round_data_keys << sender << "\n";
  }
  if (auto v = round_data_keys.str(); !v.empty()) {
    db_->batch_put(batch, DbStorage::Columns::replay_protection,
                   roundDataKeysKey(round), v);
  }
  if (round < range_) {
    return;
  }
  auto bottom_round = round - range_;
  auto bottom_round_data_keys_key = roundDataKeysKey(bottom_round);
  auto keys = db_->lookup(bottom_round_data_keys_key,
                          DbStorage::Columns::replay_protection);
  if (keys.empty()) {
    return;
  }
  istringstream is(keys);
  for (string line; getline(is, line);) {
    auto nonce_max_key = maxNonceAtRoundKey(bottom_round, line);
    if (auto v =
            db_->lookup(nonce_max_key, DbStorage::Columns::replay_protection);
        !v.empty()) {
      auto sender_state_key = senderStateKey(line);
      auto state = loadSenderState(db_, sender_state_key);
      state->nonce_watermark = stoull(v);
      db_->batch_put(batch, DbStorage::Columns::replay_protection,
                     sender_state_key, db_slice(state->rlp()));
      db_->batch_delete(batch, DbStorage::Columns::replay_protection,
                        nonce_max_key);
    }
  }
  db_->batch_delete(batch, DbStorage::Columns::replay_protection,
                    bottom_round_data_keys_key);
}

}  // namespace taraxa::replay_protection_service