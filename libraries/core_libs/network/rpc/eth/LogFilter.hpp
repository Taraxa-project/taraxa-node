#pragma once

#include "data.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::net::rpc::eth {

struct LogFilter {
  using Topics = std::array<std::unordered_set<h256>, 4>;

 private:
  EthBlockNumber from_block_;
  std::optional<EthBlockNumber> to_block_;
  AddressSet addresses_;
  Topics topics_;
  bool is_range_only_ = false;

 public:
  LogFilter(EthBlockNumber from_block, std::optional<EthBlockNumber> to_block, AddressSet addresses,
            LogFilter::Topics topics);
  std::vector<LogBloom> bloomPossibilities() const;
  bool matches(LogBloom b) const;
  void match_one(const TransactionReceipt& r, const std::function<void(size_t)>& cb) const;
  bool blk_number_matches(EthBlockNumber blk_n) const;
  void match_one(const ExtendedTransactionLocation& trx_loc, const TransactionReceipt& r,
                 const std::function<void(const LocalisedLogEntry&)>& cb) const;
  std::vector<LocalisedLogEntry> match_all(const final_chain::FinalChain& final_chain) const;
};

AddressSet parse_addresses(const Json::Value& json);
LogFilter::Topics parse_topics(const Json::Value& json);

}  // namespace taraxa::net::rpc::eth