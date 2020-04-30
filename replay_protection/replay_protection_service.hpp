#ifndef TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_

#include <libethereum/Transaction.h>

#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>

#include "db_storage.hpp"
#include "eth/database_adapter.hpp"
#include "sender_state.hpp"
#include "transaction.hpp"

namespace taraxa::replay_protection::replay_protection_service {
using dev::db::DatabaseFace;
using dev::eth::Transactions;
using eth::database_adapter::DatabaseAdapter;
using sender_state::SenderState;
using std::list;
using std::optional;
using std::shared_lock;
using std::shared_mutex;
using std::shared_ptr;
using std::string;

struct ReplayProtectionService {
 private:
  round_t range_ = 0;
  // TODO use DbStorage
  shared_ptr<DatabaseAdapter> db_;
  // TODO optimistic lock
  shared_mutex m_;

 public:
  explicit ReplayProtectionService(decltype(range_) range,
                                   shared_ptr<DbStorage> const& db_storage);
  // is_overlapped
  bool hasBeenExecutedWithinRange(Transaction const& trx);
  // is_stale
  bool hasBeenExecutedBeyondRange(Transaction const& trx);
  bool hasBeenExecuted(Transaction const& trx) {
    shared_lock l(m_);
    return hasBeenExecuted_(trx);
  }
  void commit(DbStorage::BatchPtr batch, round_t round,
              Transactions const& trxs);

 private:
  bool hasBeenExecuted_(Transaction const& trx) {
    return hasBeenExecutedWithinRange(trx) || hasBeenExecutedBeyondRange(trx);
  }
  shared_ptr<SenderState> loadSenderState(string const& key);
};

}  // namespace taraxa::replay_protection::replay_protection_service

#endif  // TARAXA_NODE_REPLAY_PROTECTION_REPLAY_PROTECTION_SERVICE_HPP_
