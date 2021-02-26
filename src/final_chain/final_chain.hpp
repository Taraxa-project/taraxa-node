#pragma once

#include "ChainDB.h"
#include "common/types.hpp"
#include "state_api.hpp"
#include "storage/db_storage.hpp"
#include "util/event.hpp"
#include "util/exit_stack.hpp"
#include "util/range_view.hpp"

namespace taraxa::final_chain {
using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::final_chain;
using namespace ::taraxa::util;

struct FinalChain : virtual ChainDB {
  struct Config {
    state_api::ChainConfig state;
    struct GenesisBlockFields {
      addr_t author;
      uint64_t timestamp = 0;
    } genesis_block_fields;
  };

  struct Opts {
    state_api::Opts state_api;
  };

  struct BlockExecuted {
    shared_ptr<PbftBlock const> pbft_blk;
    vector<blk_hash_t> finalized_dag_blk_hashes;
    shared_ptr<BlockHeader const> final_chain_blk;
    TransactionReceipts trx_receipts;
  };

 protected:
  util::EventEmitter<BlockExecuted> const block_executed_;

 public:
  decltype(block_executed_)::Subscriber const& block_executed = block_executed_;

  virtual ~FinalChain() {}

  virtual void execute(std::shared_ptr<PbftBlock> pbft_blk) = 0;
  virtual shared_ptr<BlockHeader> get_last_block() const = 0;
  virtual void create_state_db_snapshot(uint64_t const& period) = 0;
  virtual optional<state_api::Account> get_account(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key,
                                   optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual bytes get_code(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<BlockNumber> blk_n = nullopt,
                                          optional<state_api::ExecutionOptions> const& opts = nullopt) const = 0;
  virtual std::pair<val_t, bool> getBalance(addr_t const& acc) const = 0;
  virtual uint64_t dpos_eligible_count(BlockNumber blk_num) const = 0;
  virtual bool dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const = 0;
  virtual state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                                optional<BlockNumber> blk_n = nullopt) const = 0;
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db,  //
                                     shared_ptr<PbftChain> pbft_chain,
                                     FinalChain::Config const& config,   //
                                     FinalChain::Opts const& opts = {},  //
                                     addr_t const& node_addr = {});

Json::Value enc_json(FinalChain::Config const& obj);
void dec_json(Json::Value const& json, FinalChain::Config& obj);
Json::Value enc_json(FinalChain::Config::GenesisBlockFields const& obj);
void dec_json(Json::Value const& json, FinalChain::Config::GenesisBlockFields& obj);

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa
