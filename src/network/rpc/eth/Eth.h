#pragma once

#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"
#include "watches.hpp"

namespace taraxa::net::rpc::eth {
using namespace ::taraxa::final_chain;
using namespace ::std;
using namespace ::dev;

struct EthParams {
  Address address;
  Secret secret;
  uint64_t chain_id = 0;
  shared_ptr<FinalChain> final_chain;
  function<shared_ptr<Transaction>(h256 const&)> get_trx;
  function<void(Transaction const& trx)> send_trx;
  function<u256()> gas_pricer = [] { return u256(0); };
  function<bool()> syncing_probe = [] { return false; };
  WatchesConfig watches_cfg;
};

struct Eth : virtual ::taraxa::net::EthFace {
  virtual ~Eth() {}

  virtual void note_block_executed(BlockHeader const&, Transactions const&, TransactionReceipts const&) = 0;
  virtual void note_pending_transaction(h256 const& trx_hash) = 0;
};

std::shared_ptr<Eth> NewEth(EthParams&&);

Json::Value toJson(BlockHeader const& obj);

}  // namespace taraxa::net::rpc::eth
