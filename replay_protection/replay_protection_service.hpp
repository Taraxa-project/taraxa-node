#ifndef TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_

#include <libdevcore/RocksDB.h>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include "sender_state.hpp"
#include "transaction.hpp"

namespace taraxa::replay_protection::replay_protection_service {
using dev::db::RocksDB;
using sender_state::SenderState;
using std::list;
using std::optional;
using std::shared_mutex;
using std::shared_ptr;
using std::string;

struct ReplayProtectionService {
  using transaction_batch_t = list<shared_ptr<Transaction>>;

 private:
  round_t range_ = 0;
  // explicitly use rocksdb since WriteBatchFace may behave not as expected
  // in other implementations
  shared_ptr<RocksDB> db_;
  optional<round_t> last_round_;
  shared_mutex m_;

 public:
  ReplayProtectionService(decltype(range_) range, decltype(db_) const& db);
  bool hasBeenExecuted(Transaction const& trx);
  void commit(round_t round, transaction_batch_t const& trxs);

 private:
  bool hasBeenExecuted_(Transaction const& trx);
  shared_ptr<SenderState> loadSenderState(string const& key);
};

}  // namespace taraxa::replay_protection::replay_protection_service

#endif  // TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_
