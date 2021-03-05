#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>

#include <iosfwd>
#include <memory>
#include <optional>

#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"

namespace taraxa::net::rpc::eth {

Json::Value toJson(final_chain::BlockHeader const& obj);

struct EthParams {
  dev::Address address;
  dev::Secret secret;
  uint64_t chain_id = 0;
  std::shared_ptr<FinalChain> final_chain;
  std::function<std::shared_ptr<Transaction>(h256 const&)> get_trx;
  std::function<h256 const&(Transaction const& trx)> send_trx;
  std::function<dev::u256()> gas_pricer = [] { return dev::u256(0); };
};

struct Eth : virtual ::taraxa::net::EthFace {
  virtual ~Eth() {}

  virtual void note_block(dev::h256 const& blk_hash) = 0;
  virtual void note_pending_transactions(util::RangeView<dev::h256> const& trx_hashes) = 0;
  virtual void note_receipts(util::RangeView<final_chain::TransactionReceipt> const& receipts) = 0;
};

Eth* NewEth(EthParams&&);

}  // namespace taraxa::net::rpc::eth
