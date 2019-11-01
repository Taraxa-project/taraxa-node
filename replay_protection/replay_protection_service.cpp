#include "replay_protection_service.hpp"
#include <libdevcore/RLP.h>
#include <sstream>
#include <unordered_map>
#include "util/eth.hpp"

namespace taraxa::replay_protection::replay_protection_service {
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
using util::eth::toSlice;

string const CURR_ROUND_KEY = "curr_rnd";

string senderStateKey(string const& sender_str) {
  return "sender_" + sender_str;
}

string roundDataKeysKey(round_t round) {
  return "data_keys_at" + to_string(round);
}

string trxHashKey(string const& trx_hash_str) { return "trx_" + trx_hash_str; }

string maxNonceAtRoundKey(round_t round, string const& sender_addr) {
  return "max_nonce_at_" + to_string(round) + "_" + sender_addr;
}

ReplayProtectionService::ReplayProtectionService(decltype(range_) range,
                                                 decltype(db_) const& db)
    : range_(range), db_(db) {
  if (auto v = db_->lookup(CURR_ROUND_KEY); !v.empty()) {
    last_round_ = stoull(v);
  }
}

bool ReplayProtectionService::hasBeenExecuted(Transaction const& trx) {
  shared_lock l(m_);
  if (!db_->lookup(trxHashKey(trx.getHash().hex())).empty()) {
    return true;
  }
  auto sender_state = loadSenderState(trx.sender().hex());
  if (!sender_state) {
    return false;
  }
  auto nonce_watermark = sender_state->getNonceWatermark();
  return nonce_watermark && trx.getNonce() <= *nonce_watermark;
}

void ReplayProtectionService::commit(round_t round,
                                     transaction_batch_t const& trxs) {
  unique_lock l(m_);
  if (last_round_) {
    assert(*last_round_ + 1 == round);
  } else {
    assert(round == 0);
  }
  optional<round_t> bottom_round =
      round >= range_ ? optional(round - range_) : nullopt;
  auto batch = db_->createWriteBatch();
  stringstream round_data_keys;
  unordered_map<string, shared_ptr<SenderState>> sender_states;
  for (auto const& trx : trxs) {
    auto sender_str = trx->sender().hex();
    shared_ptr<SenderState> sender_state;
    if (auto e = sender_states.find(sender_str); e != sender_states.end()) {
      sender_state = e->second;
    } else {
      if (sender_state = loadSenderState(sender_str); !sender_state) {
        sender_state = make_shared<SenderState>();
      }
      sender_states[sender_str] = sender_state;
    }
    sender_state->setNonceMax(trx->getNonce());
    auto trx_hash_str = trx->getHash().hex();
    static string DUMMY_VALUE = "_";
    batch->insert(trxHashKey(trx_hash_str), DUMMY_VALUE);
    round_data_keys << trx_hash_str << "\n";
  }
  for (auto const& [sender, state] : sender_states) {
    if (bottom_round) {
      auto v = db_->lookup(maxNonceAtRoundKey(*bottom_round, sender));
      if (!v.empty()) {
        state->setNonceWatermark(fromBigEndian<trx_nonce_t>(v));
      }
    }
    if (state->isDirty()) {
      batch->insert(senderStateKey(sender), toSlice(state->rlp().out()));
    }
    if (state->isNonceMaxDirty() || state->isDefaultInitialized()) {
      batch->insert(maxNonceAtRoundKey(round, sender),
                    toBigEndianString(state->getNonceMax()));
      round_data_keys << sender << "\n";
    }
  }
  batch->insert(roundDataKeysKey(round), round_data_keys.str());
  if (bottom_round) {
    // cleanup
    auto bottom_round_data_keys_key = roundDataKeysKey(*bottom_round);
    if (auto keys = db_->lookup(bottom_round_data_keys_key); !keys.empty()) {
      istringstream is(keys);
      for (string line; getline(is, line);) {
        switch (line.size()) {
          case addr_t::size:
            batch->kill(maxNonceAtRoundKey(*bottom_round, line));
            break;
          case trx_hash_t::size:
            batch->kill(trxHashKey(line));
            break;
        }
      }
      batch->kill(bottom_round_data_keys_key);
    }
  }
  batch->insert(CURR_ROUND_KEY, to_string(round));
  db_->commit(move(batch));
  last_round_ = round;
}

shared_ptr<SenderState> ReplayProtectionService::loadSenderState(
    string const& sender_str) {
  if (auto v = db_->lookup(senderStateKey(sender_str)); !v.empty()) {
    return make_shared<SenderState>(RLP(v));
  }
  return nullptr;
}

}  // namespace taraxa::replay_protection::replay_protection_service