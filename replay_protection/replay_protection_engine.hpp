#ifndef TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_ENGINE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_ENGINE_HPP_

#include <libdevcore/RLP.h>
#include <libdevcore/RocksDB.h>
#include <algorithm>
#include <list>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include "transaction.hpp"
#include "util/eth.hpp"

namespace taraxa::replay_protection::replay_protection_engine {
using dev::fromBigEndian;
using dev::RLP;
using dev::RLPStream;
using dev::toBigEndianString;
using dev::db::RocksDB;
using std::getline;
using std::istringstream;
using std::list;
using std::make_shared;
using std::move;
using std::nullopt;
using std::optional;
using std::shared_lock;
using std::shared_mutex;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::to_string;
using std::unique_lock;
using std::unordered_map;
using util::eth::toSlice;

class SenderState {
  // persistent
  trx_nonce_t nonce_max;
  trx_nonce_t nonce_watermark;
  bool nonce_watermark_exists;
  // transient
  bool dirty;
  bool nonce_max_dirty;
  bool default_initialized;

 public:
  SenderState() : default_initialized(true) {}

  explicit SenderState(RLP const& rlp)
      : nonce_max(rlp[0].toInt<trx_nonce_t>()),
        nonce_watermark(rlp[1].toInt<trx_nonce_t>()),
        nonce_watermark_exists(rlp[2].toInt<bool>()) {}

  auto isDefaultInitialized() { return default_initialized; };

  auto isDirty() { return dirty; }

  auto isNonceMaxDirty() { return nonce_max_dirty; }

  auto getNonceMax() { return nonce_max; }

  bool setNonceMax(trx_nonce_t v) {
    if (nonce_max >= v) {
      return false;
    }
    nonce_max = v;
    dirty = nonce_max_dirty = true;
    return true;
  }

  optional<trx_nonce_t> getNonceWatermark() {
    return nonce_watermark_exists ? optional(nonce_watermark) : nullopt;
  };

  void setNonceWatermark(trx_nonce_t v) {
    if (nonce_watermark == v && nonce_watermark_exists) {
      return;
    }
    nonce_watermark = v;
    dirty = nonce_watermark_exists = true;
  }

  RLPStream rlp() {
    return RLPStream(3) << nonce_max << nonce_watermark
                        << nonce_watermark_exists;
  }
};

struct RoundState {};

class ReplayProtectionEngine {
  round_t range_;
  // explicitly use rocksdb since WriteBatchFace may behave not as expected
  // in other implementations
  shared_ptr<RocksDB> db_;
  optional<round_t> last_round_;
  shared_mutex m_;

 public:
  ReplayProtectionEngine(decltype(range_) range, decltype(db_) const& db)
      : range_(range), db_(db) {
    if (auto v = db_->lookup(CURR_ROUND_KEY); !v.empty()) {
      last_round_ = stoull(v);
    }
  }

  // TODO methods for packing and excluding blocks from dag

  optional<trx_nonce_t> getNonceWatermark(addr_t const& sender) {
    if (auto v = loadSenderState(sender.hex()); v) {
      return v->getNonceWatermark();
    }
    return nullopt;
  }

  bool hasBeenCommittedWithinRange(trx_hash_t const& trx_hash) {
    return !db_->lookup(trxHashKey(trx_hash.hex())).empty();
  }

  // to be called from Executor::execute
  void commit(round_t round, list<shared_ptr<Transaction>> const& trxs) {
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

 private:
  static inline string const CURR_ROUND_KEY = "curr_rnd";

  shared_ptr<SenderState> loadSenderState(string const& sender_str) {
    if (auto v = db_->lookup(senderStateKey(sender_str)); !v.empty()) {
      return make_shared<SenderState>(RLP(v));
    }
    return nullptr;
  }

  static string senderStateKey(string const& sender_str) {
    return "sender_" + sender_str;
  }

  static string roundDataKeysKey(round_t round) {
    return "data_keys_at" + to_string(round);
  }

  static string trxHashKey(string const& trx_hash_str) {
    return "trx_" + trx_hash_str;
  }

  static string maxNonceAtRoundKey(round_t round, string const& sender_addr) {
    return "max_nonce_at_" + to_string(round) + "_" + sender_addr;
  }
};

}  // namespace taraxa::replay_protection::replay_protection_engine

#endif  // TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_ENGINE_HPP_
