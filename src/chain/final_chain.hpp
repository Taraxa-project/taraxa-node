#pragma once

#include <libethereum/ChainDBImpl.h>

#include "aleth/database.hpp"
#include "common/types.hpp"
#include "state_api.hpp"
#include "storage/db_storage.hpp"
#include "util/exit_stack.hpp"
#include "util/range_view.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

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

  virtual ~FinalChain() {}

  struct AdvanceResult {
    BlockHeader new_header;
    TransactionReceipts const& receipts;
    state_api::StateTransitionResult const& state_transition_result;
  };
  virtual AdvanceResult advance(DbStorage::BatchPtr batch, Address const& author, uint64_t timestamp,
                                Transactions const& transactions) = 0;
  virtual shared_ptr<BlockHeader> get_last_block() const = 0;
  virtual void advance_confirm() = 0;
  virtual void create_snapshot(uint64_t const& period) = 0;
  virtual optional<state_api::Account> get_account(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key,
                                   optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual bytes get_code(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<BlockNumber> blk_n = nullopt,
                                          optional<state_api::ExecutionOptions> const& opts = nullopt) const = 0;
  virtual std::pair<val_t, bool> getBalance(addr_t const& acc) const = 0;
  virtual uint64_t dpos_eligible_count(BlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_total_vote_count(BlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_vote_count(BlockNumber blk_num, addr_t const& addr) const = 0;

  virtual bool dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const = 0;
  virtual state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                                optional<BlockNumber> blk_n = nullopt) const = 0;
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db, FinalChain::Config const& config,
                                     FinalChain::Opts const& opts = {});

Json::Value enc_json(FinalChain::Config const& obj);
void dec_json(Json::Value const& json, FinalChain::Config& obj);
Json::Value enc_json(FinalChain::Config::GenesisBlockFields const& obj);
void dec_json(Json::Value const& json, FinalChain::Config::GenesisBlockFields& obj);

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa
