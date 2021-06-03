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

  virtual bool is_nonce_stale(addr_t const& addr, uint64_t nonce) const = 0;

  struct TransactionInfo {
    addr_t sender;
    uint64_t nonce = 0;
  };
  virtual void update(DB::Batch& batch, uint64_t period, util::RangeView<TransactionInfo> const& trxs) = 0;
};

struct ReplayProtectionServiceDummy : ReplayProtectionService {
  bool is_nonce_stale(addr_t const& addr, uint64_t nonce) const override { return false; }
  void update(DB::Batch& batch, uint64_t period, util::RangeView<TransactionInfo> const& trxs) override {}
};

std::unique_ptr<ReplayProtectionService> NewReplayProtectionService(ReplayProtectionService::Config config,
                                                                    std::shared_ptr<DB> db);

Json::Value enc_json(ReplayProtectionService::Config const& obj);
void dec_json(Json::Value const& json, ReplayProtectionService::Config& obj);

}  // namespace taraxa::final_chain
