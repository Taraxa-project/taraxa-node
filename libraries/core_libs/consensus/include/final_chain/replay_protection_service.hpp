#pragma once

#include <memory>

#include "common/range_view.hpp"
#include "storage/storage.hpp"

namespace taraxa::final_chain {

struct ReplayProtectionService {
  struct Config {
    uint64_t range = 0;
  };

  virtual ~ReplayProtectionService() = default;

  virtual bool is_nonce_stale(const addr_t& addr, const trx_nonce_t& nonce) const = 0;

  struct TransactionInfo {
    addr_t sender;
    trx_nonce_t nonce = 0;
  };
  virtual void update(DB::Batch&, uint64_t, util::RangeView<TransactionInfo> const&) = 0;
};

std::unique_ptr<ReplayProtectionService> NewReplayProtectionService(ReplayProtectionService::Config,
                                                                    std::shared_ptr<DB>);

Json::Value enc_json(ReplayProtectionService::Config const&);
void dec_json(Json::Value const&, ReplayProtectionService::Config&);

}  // namespace taraxa::final_chain
