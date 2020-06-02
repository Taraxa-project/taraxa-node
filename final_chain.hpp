#ifndef TARAXA_NODE_FINAL_CHAIN_HPP_
#define TARAXA_NODE_FINAL_CHAIN_HPP_

#include <libethereum/ChainDBImpl.h>

#include "aleth/database.hpp"
#include "db_storage.hpp"
#include "replay_protection_service.hpp"
#include "state_api.hpp"
#include "types.hpp"
#include "util/exit_stack.hpp"
#include "util/range_view.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

struct FinalChain : virtual ChainDB {
  struct Config {
    struct StateConfig {
      state_api::InputAccounts genesis_accounts;
      state_api::ChainConfig chain_config;
    } state;
    struct GenesisBlockFields {
      addr_t author;
      uint64_t timestamp;
    } genesis_block_fields;
  };

  struct Opts {
    state_api::CacheOpts state_api;
  };

  virtual ~FinalChain() {}

  struct AdvanceResult {
    BlockHeader new_header;
    TransactionReceipts const& receipts;
    state_api::StateTransitionResult const& state_transition_result;
  };
  virtual AdvanceResult advance(DbStorage::BatchPtr batch,
                                Address const& author, uint64_t timestamp,
                                Transactions const& transactions) = 0;
  virtual shared_ptr<BlockHeader> get_last_block() const = 0;
  virtual void advance_confirm() = 0;
  virtual optional<state_api::Account> get_account(
      addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual u256 get_account_storage(
      addr_t const& addr, u256 const& key,
      optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual bytes get_code(addr_t const& addr,
                         optional<BlockNumber> blk_n = nullopt) const = 0;
  virtual state_api::ExecutionResult call(
      state_api::EVMTransaction const& trx,
      optional<BlockNumber> blk_n = nullopt,
      optional<state_api::ExecutionOptions> const& opts = nullopt) const = 0;
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db,
                                     FinalChain::Config const& config,
                                     FinalChain::Opts const& opts = {});

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa

#endif  // TARAXA_NODE_FINAL_CHAIN_HPP_
