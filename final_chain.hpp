#ifndef TARAXA_NODE_FINAL_CHAIN_HPP_
#define TARAXA_NODE_FINAL_CHAIN_HPP_

#include <libethereum/BlockChain.h>

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

struct FinalChain {
  using BlockDB = dev::eth::BlockChain;

  struct Config {
    round_t replay_protection_service_range;
    struct StateConfig {
      StateAPI::InputAccounts genesis_accounts;
      StateAPI::ChainConfig chain_config;
    } state;
    struct GenesisBlockFields {
      addr_t author;
      uint64_t timestamp;
    } genesis_block_fields;
  };

 private:
  shared_ptr<aleth::Database> bc_db;
  shared_ptr<aleth::Database> bc_extras_db;
  shared_ptr<BlockDB> bc;
  shared_ptr<ReplayProtectionService> replay_protection_service;
  shared_ptr<StateAPI> state_api;

 public:
  FinalChain(shared_ptr<DbStorage> db, Config const& config);

  auto get_state_api() { return state_api; }
  auto get_block_db() { return bc; }

  struct AdvanceResult {
    BlockHeader new_header;
    TransactionReceipts receipts;
    unordered_map<addr_t, u256> balance_changes;
  };
  AdvanceResult advance(DbStorage::BatchPtr batch, Address const& author,
                        uint64_t timestamp, Transactions const& transactions);

  void advance_confirm();

  auto get_last_block() { return bc->get_last_block(); }
  trx_nonce_t accountNonce(addr_t const&) { return 0; }

  bool has_been_executed(h256 const& trx_hash) { return false; }

  bool has_been_executed_or_nonce_stale(h256 const& trx_hash,
                                        addr_t const& from,
                                        trx_nonce_t const& nonce) {
    assert(nonce <= numeric_limits<uint64_t>::max());
    return has_been_executed(trx_hash) ||
           replay_protection_service->is_nonce_stale(
               from, nonce.convert_to<uint64_t>());
  }

 private:
  util::ExitStack append_block_prepare(DbStorage::BatchPtr batch);
};

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
}

#endif  // TARAXA_NODE_FINAL_CHAIN_HPP_
