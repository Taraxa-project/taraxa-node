#pragma once

#include "final_chain/final_chain.hpp"
#include "network/rpc/EthFace.h"

namespace taraxa::net::rpc::eth {
using namespace ::taraxa::final_chain;
using namespace ::std;
using namespace ::dev;

struct WatchGroupConfig {
  uint64_t max_watches = 0;
  chrono::seconds idle_timeout{5 * 60};
};

struct WatchesConfig {
  WatchGroupConfig new_blocks;
  WatchGroupConfig new_transactions;
  WatchGroupConfig logs;
};

struct EthParams {
  Address address;
  Secret secret;
  uint64_t chain_id = 0;
  shared_ptr<FinalChain> final_chain;
  function<shared_ptr<Transaction>(h256 const&)> get_trx;
  function<h256 const&(Transaction const& trx)> send_trx;
  function<u256()> gas_pricer = [] { return u256(0); };
  WatchesConfig watches_cfg;
};

struct Eth : virtual ::taraxa::net::EthFace {
  virtual ~Eth() {}

  virtual void note_block(h256 const& blk_hash) = 0;
  virtual void note_pending_transactions(util::RangeView<h256> const& trx_hashes) = 0;
  virtual void note_receipts(util::RangeView<TransactionReceipt> const& receipts) = 0;
};

Eth* NewEth(EthParams&&);

Json::Value toJson(BlockHeader const& obj);

}  // namespace taraxa::net::rpc::eth
