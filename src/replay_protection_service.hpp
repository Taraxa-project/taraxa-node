#ifndef TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_
#define TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_

#include <memory>

#include "db_storage.hpp"
#include "util/range_view.hpp"

namespace taraxa {

struct ReplayProtectionService {
  struct Config {
    round_t range = 0;
  };

  virtual ~ReplayProtectionService() {}

  virtual bool is_nonce_stale(addr_t const& addr, uint64_t nonce) const = 0;

  struct TransactionInfo {
    addr_t sender;
    uint64_t nonce = 0;
  };
  virtual void update(DbStorage::BatchPtr batch, round_t round, util::RangeView<TransactionInfo> const& trxs) = 0;
};

std::unique_ptr<ReplayProtectionService> NewReplayProtectionService(ReplayProtectionService::Config config, std::shared_ptr<DbStorage> db);

Json::Value enc_json(ReplayProtectionService::Config const& obj);
void dec_json(Json::Value const& json, ReplayProtectionService::Config& obj);

}  // namespace taraxa

#endif  // TARAXA_NODE_REPLAY_PROTECTION_SERVICE_HPP_
