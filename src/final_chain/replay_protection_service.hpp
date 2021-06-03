#pragma once

#include <memory>

#include "storage/db_storage.hpp"
#include "util/range_view.hpp"

namespace taraxa::final_chain {

struct ReplayProtectionService {
  struct Config {
    uint64_t range = 0;
  };

  virtual ~ReplayProtectionService() {}

  virtual bool is_nonce_stale(addr_t const&, uint64_t) const = 0;

  struct TransactionInfo {
    addr_t sender;
    uint64_t nonce = 0;
  };
  virtual void update(DB::Batch&, uint64_t, util::RangeView<TransactionInfo> const&) = 0;
};

struct ReplayProtectionServiceDummy : ReplayProtectionService {
  bool is_nonce_stale(addr_t const&, uint64_t) const override { return false; }
  void update(DB::Batch&, uint64_t, util::RangeView<TransactionInfo> const&) override {}
};

std::unique_ptr<ReplayProtectionService> NewReplayProtectionService(ReplayProtectionService::Config,
                                                                    std::shared_ptr<DB>);

Json::Value enc_json(ReplayProtectionService::Config const&);
void dec_json(Json::Value const&, ReplayProtectionService::Config&);

}  // namespace taraxa::final_chain
