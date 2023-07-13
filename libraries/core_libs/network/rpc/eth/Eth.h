#pragma once

#include "data.hpp"
#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"
#include "watches.hpp"

namespace taraxa::net::rpc::eth {

struct EthParams {
  Address address;
  uint64_t chain_id = 0;
  uint64_t gas_limit = ((uint64_t)1 << 53) - 1;
  std::shared_ptr<FinalChain> final_chain;
  std::function<std::shared_ptr<Transaction>(const h256&)> get_trx;
  std::function<void(const std::shared_ptr<Transaction>& trx)> send_trx;
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
  Eth& operator=(Eth&& rhs) {
    ::taraxa::net::EthFace::operator=(std::move(rhs));
    return *this;
  }
  virtual void note_block_executed(const final_chain::BlockHeader&, const SharedTransactions&,
                                   const final_chain::TransactionReceipts&) = 0;
  virtual void note_pending_transaction(const h256& trx_hash) = 0;
};

std::shared_ptr<Eth> NewEth(EthParams&&);

}  // namespace taraxa::net::rpc::eth
