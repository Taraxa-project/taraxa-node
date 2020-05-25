#ifndef TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_

#include <libethereum/Transaction.h>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "db_storage.hpp"
#include "util/range_view.hpp"

namespace taraxa::replay_protection_service {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

class ReplayProtectionService {
  round_t range_ = 0;
  // TODO use DbStorage
  shared_ptr<DbStorage> db_;
  // TODO optimistic lock
  shared_mutex m_;

 public:
  ReplayProtectionService(decltype(range_) range,
                          shared_ptr<DbStorage> const& db_storage);
  bool is_nonce_stale(addr_t const& addr, uint64_t nonce);

  struct TransactionInfo {
    addr_t sender;
    uint64_t nonce;
  };
  void register_executed_transactions(DbStorage::BatchPtr batch, round_t round,
                                      RangeView<TransactionInfo> const& trxs);
};

}  // namespace taraxa::replay_protection_service

namespace taraxa {
using replay_protection_service::ReplayProtectionService;
}

#endif  // TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_
