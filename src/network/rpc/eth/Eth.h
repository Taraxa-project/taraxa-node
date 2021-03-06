#pragma once

#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"

namespace taraxa::net::rpc::eth {
using namespace ::taraxa::final_chain;
using namespace ::std;
using namespace ::dev;

enum WatchType {
  new_blocks,
  new_transactions,
  logs,

  // do not touch
  COUNT,
};
struct WatchGroupConfig {
  uint64_t max_watches = 0;
  chrono::seconds idle_timeout{5 * 60};
};
using WatchesConfig = array<WatchGroupConfig, WatchType::COUNT>;

struct EthParams {
  Address address;
  Secret secret;
  uint64_t chain_id = 0;
  shared_ptr<FinalChain> final_chain;
  function<shared_ptr<Transaction>(h256 const&)> get_trx;
  function<void(Transaction const& trx)> send_trx;
  function<u256()> gas_pricer = [] { return u256(0); };
  WatchesConfig watches_cfg;
};

struct Eth : virtual ::taraxa::net::EthFace {
  virtual ~Eth() {}

  virtual void note_block_executed(BlockHeader const&, Transactions const&, TransactionReceipts const&) = 0;
  virtual void note_pending_transactions(util::RangeView<h256> const& trx_hashes) = 0;
};

Eth* NewEth(EthParams&&);

Json::Value toJson(BlockHeader const& obj);

}  // namespace taraxa::net::rpc::eth
