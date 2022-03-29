#pragma once

#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"
#include "watches.hpp"

namespace taraxa::net::rpc::eth {

struct EthParams {
  Address address;
  Secret secret;
  uint64_t chain_id = 0;
  std::shared_ptr<FinalChain> final_chain;
  std::function<std::shared_ptr<Transaction>(h256 const&)> get_trx;
  std::function<void(Transaction const& trx)> send_trx;
  std::function<u256()> gas_pricer = [] { return u256(0); };
  std::function<std::optional<SyncStatus>()> syncing_probe = [] { return std::nullopt; };
  WatchesConfig watches_cfg;
};

struct Eth : virtual ::taraxa::net::EthFace {
  Eth() = default;
  virtual ~Eth() = default;

  Eth(const Eth&) = default;
  Eth(Eth&&) = default;
  Eth& operator=(const Eth&) = default;
  Eth& operator=(Eth&&) = default;

  virtual void note_block_executed(final_chain::BlockHeader const&, Transactions const&,
                                   final_chain::TransactionReceipts const&) = 0;
  virtual void note_pending_transaction(h256 const& trx_hash) = 0;
};

std::shared_ptr<Eth> NewEth(EthParams&&);

Json::Value toJson(final_chain::BlockHeader const& obj);

}  // namespace taraxa::net::rpc::eth
