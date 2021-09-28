#pragma once

#include "data.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::net::rpc::eth {

struct LogFilter {
  using Topics = array<unordered_set<h256>, 4>;

 private:
  EthBlockNumber from_block_;
  optional<EthBlockNumber> to_block_;
  AddressSet addresses_;
  Topics topics_;
  bool is_range_only_ = false;

 public:
  LogFilter(EthBlockNumber from_block, optional<EthBlockNumber> to_block, AddressSet addresses,
            LogFilter::Topics topics);

  vector<LogBloom> bloomPossibilities() const;
  bool matches(LogBloom b) const;
  void match_one(TransactionReceipt const& r, function<void(size_t)> const& cb) const;

 public:
  bool blk_number_matches(EthBlockNumber blk_n) const;
  void match_one(ExtendedTransactionLocation const& trx_loc, TransactionReceipt const& r,
                 function<void(LocalisedLogEntry const&)> const& cb) const;
  vector<LocalisedLogEntry> match_all(FinalChain const& final_chain) const;
};

}  // namespace taraxa::net::rpc::eth