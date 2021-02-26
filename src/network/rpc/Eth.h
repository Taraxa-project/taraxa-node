#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>

#include <iosfwd>
#include <memory>
#include <optional>

#include "EthFace.h"
#include "final_chain/final_chain.hpp"

namespace taraxa::net {

Json::Value toJson(final_chain::BlockHeader const& _t);
Json::Value toJson(final_chain::LocalisedLogEntry const& _t);
Json::Value toJson(final_chain::LocalisedTransaction const& _t);
Json::Value toJson(final_chain::LocalisedTransactionReceipt const& _t);
Json::Value toJson(final_chain::BlockHeaderWithTransactions const& obj);
Json::Value toJson(final_chain::TransactionSkeleton const& _t);

struct EthParams {
  dev::Address address;
  dev::Secret secret;
  uint64_t chain_id = 0;
  std::shared_ptr<FinalChain> final_chain;
  std::function<h256 const&(Transaction const& trx)> send_trx;
  std::function<dev::u256()> gas_pricer = [] { return dev::u256(0); };
};

struct Eth : virtual EthFace {
  virtual ~Eth() {}

  virtual void note_block(dev::h256 const& blk_hash) = 0;
  virtual void note_pending_transactions(util::RangeView<dev::h256> const& trx_hashes) = 0;
  virtual void note_receipts(util::RangeView<final_chain::TransactionReceipt> const& receipts) = 0;
};

Eth* NewEth(EthParams&&);

}  // namespace taraxa::net
