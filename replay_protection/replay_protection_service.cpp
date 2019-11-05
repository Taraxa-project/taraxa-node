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
using taraxa::as_shared;
using util::eth::toSlice;

string const CURR_ROUND_KEY = "curr_rnd";

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

ReplayProtectionService::ReplayProtectionService(decltype(range_) range,
                                                 decltype(db_) const& db)
    : range_(range), db_(db) {
  assert(range_ > 0);
  if (auto v = db_->lookup(CURR_ROUND_KEY); !v.empty()) {
    last_round_ = stoull(v);
  }
}

bool ReplayProtectionService::hasBeenExecuted(Transaction const& trx) {
  shared_lock l(m_);
  return hasBeenExecuted_(trx);
}

void ReplayProtectionService::commit(round_t round,
                                     transaction_batch_t const& trxs) {
  unique_lock l(m_);
  TARAXA_PARANOID_CHECK {
    if (last_round_) {
      assert(*last_round_ + 1 == round);
    } else {
      assert(round == 0);
    }
    for (auto const& trx : trxs) {
      assert(!hasBeenExecuted_(*trx));
    }
  }
  auto batch = db_->createWriteBatch();
  stringstream round_data_keys;
  unordered_map<string, shared_ptr<SenderState>> sender_states;
  for (auto const& trx : trxs) {
    auto sender_addr = trx->sender().hex();
    shared_ptr<SenderState> sender_state;
    if (auto e = sender_states.find(sender_addr); e != sender_states.end()) {
      sender_state = e->second;
    } else {
      sender_state = loadSenderState(senderStateKey(sender_addr));
      if (!sender_state) {
        sender_state = as_shared(new SenderState);
      }
      sender_states[sender_addr] = sender_state;
    }
    sender_state->setNonceMax(trx->getNonce());
    auto trx_hash = trx->getHash().hex();
    static string DUMMY_VALUE = "_";
    batch->insert(trxHashKey(trx_hash), DUMMY_VALUE);
    round_data_keys << trx_hash << "\n";
  }
  for (auto const& [sender, state] : sender_states) {
    if (state->isNonceMaxDirty() || state->isDefaultInitialized()) {
      batch->insert(maxNonceAtRoundKey(round, sender),
                    toBigEndianString(state->getNonceMax()));
      batch->insert(senderStateKey(sender), toSlice(state->rlp().out()));
      round_data_keys << sender << "\n";
    }
  }
  if (auto v = round_data_keys.str(); !v.empty()) {
    batch->insert(roundDataKeysKey(round), v);
  }
  if (round >= range_) {
    auto bottom_round = round - range_;
    auto bottom_round_data_keys_key = roundDataKeysKey(bottom_round);
    if (auto keys = db_->lookup(bottom_round_data_keys_key); !keys.empty()) {
      istringstream is(keys);
      for (string line; getline(is, line);) {
        auto line_size_bytes = line.size() / 2;
        if (addr_t::size == line_size_bytes) {
          auto nonce_max_key = maxNonceAtRoundKey(bottom_round, line);
          if (auto v = db_->lookup(nonce_max_key); !v.empty()) {
            auto sender_state_key = senderStateKey(line);
            auto state = loadSenderState(sender_state_key);
            state->setNonceWatermark(fromBigEndian<trx_nonce_t>(v));
            batch->insert(sender_state_key, toSlice(state->rlp().out()));
            batch->kill(nonce_max_key);
          }
        } else if (trx_hash_t::size == line_size_bytes) {
          batch->kill(trxHashKey(line));
        }
      }
      batch->kill(bottom_round_data_keys_key);
    }
  }
  batch->insert(CURR_ROUND_KEY, to_string(round));
  db_->commit(move(batch));
  last_round_ = round;
}

bool ReplayProtectionService::hasBeenExecuted_(Transaction const& trx) {
  if (!db_->lookup(trxHashKey(trx.getHash().hex())).empty()) {
    return true;
  }
  auto sender_state = loadSenderState(senderStateKey(trx.sender().hex()));
  if (!sender_state) {
    return false;
  }
  auto nonce_watermark = sender_state->getNonceWatermark();
  return nonce_watermark && trx.getNonce() <= *nonce_watermark;
}

shared_ptr<SenderState> ReplayProtectionService::loadSenderState(
    string const& key) {
  if (auto v = db_->lookup(key); !v.empty()) {
    return as_shared(new SenderState(RLP(v)));
  }
  return nullptr;
}

}  // namespace taraxa::replay_protection::replay_protection_service