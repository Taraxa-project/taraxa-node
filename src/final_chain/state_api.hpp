#pragma once

#include <rocksdb/db.h>
#include <taraxa-evm/taraxa-evm.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/types.hpp"
#include "data.hpp"
#include "storage/db_storage.hpp"
#include "util/encoding_rlp.hpp"
#include "util/range_view.hpp"

namespace taraxa::state_api {
using namespace ::dev;
using namespace ::std;
using namespace ::taraxa::util;
using namespace ::taraxa::final_chain;

static constexpr auto BlockNumberNIL = std::numeric_limits<BlockNumber>::max();

struct TaraxaEVMError : std::runtime_error {
  string const type;
  TaraxaEVMError(string type, string msg) : runtime_error(move(msg)), type(move(type)) {}
  ~TaraxaEVMError() throw() {}
};
struct ErrFutureBlock : TaraxaEVMError {
  using TaraxaEVMError::TaraxaEVMError;
  ~ErrFutureBlock() throw() {}
};

struct ExecutionOptions {
  bool disable_nonce_check = 0;
  bool disable_gas_fee = 0;

  RLP_FIELDS(disable_nonce_check, disable_gas_fee)
};
Json::Value enc_json(ExecutionOptions const& obj);
void dec_json(Json::Value const& json, ExecutionOptions& obj);

struct ETHChainConfig {
  BlockNumber homestead_block = 0;
  BlockNumber dao_fork_block = 0;
  BlockNumber eip_150_block = 0;
  BlockNumber eip_158_block = 0;
  BlockNumber byzantium_block = 0;
  BlockNumber constantinople_block = 0;
  BlockNumber petersburg_block = 0;

  RLP_FIELDS(homestead_block, dao_fork_block, eip_150_block, eip_158_block, byzantium_block, constantinople_block,
             petersburg_block)
};
Json::Value enc_json(ETHChainConfig const& obj);
void dec_json(Json::Value const& json, ETHChainConfig& obj);

using BalanceMap = unordered_map<addr_t, u256>;
Json::Value enc_json(BalanceMap const& obj);
void dec_json(Json::Value const& json, BalanceMap& obj);

struct DPOSConfig {
  u256 eligibility_balance_threshold;
  BlockNumber deposit_delay = 0;
  BlockNumber withdrawal_delay = 0;
  unordered_map<addr_t, BalanceMap> genesis_state;

  RLP_FIELDS(eligibility_balance_threshold, deposit_delay, withdrawal_delay, genesis_state)
};
Json::Value enc_json(DPOSConfig const& obj);
void dec_json(Json::Value const& json, DPOSConfig& obj);

struct DPOSTransfer {
  u256 value;
  bool negative = 0;

  RLP_FIELDS(value, negative)
};

using DPOSTransfers = unordered_map<addr_t, DPOSTransfer>;

struct ChainConfig {
  ETHChainConfig eth_chain_config;
  bool disable_block_rewards = 0;
  ExecutionOptions execution_options;
  BalanceMap genesis_balances;
  optional<DPOSConfig> dpos;

  RLP_FIELDS(eth_chain_config, disable_block_rewards, execution_options, genesis_balances, dpos)

  u256 effective_genesis_balance(addr_t const& addr) const;
};
Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

struct EVMBlock {
  addr_t Author;
  gas_t GasLimit = 0;
  uint64_t Time = 0;
  u256 Difficulty;

  RLP_FIELDS(Author, GasLimit, Time, Difficulty)
};

struct EVMTransaction {
  addr_t From;
  u256 GasPrice;
  optional<addr_t> To;
  uint64_t Nonce = 0;
  u256 Value;
  gas_t Gas = 0;
  bytes Input;

  RLP_FIELDS(From, GasPrice, To, Nonce, Value, Gas, Input)
};

struct UncleBlock {
  BlockNumber Number = 0;
  addr_t Author;

  RLP_FIELDS(Number, Author)
};

struct LogRecord {
  addr_t Address;
  vector<h256> Topics;
  bytes Data;

  RLP_FIELDS(Address, Topics, Data)
};

struct ExecutionResult {
  bytes CodeRet;
  addr_t NewContractAddr;
  vector<LogRecord> Logs;
  gas_t GasUsed = 0;
  string CodeErr;
  string ConsensusErr;

  RLP_FIELDS(CodeRet, NewContractAddr, Logs, GasUsed, CodeErr, ConsensusErr)
};

struct StateTransitionResult {
  vector<ExecutionResult> ExecutionResults;
  h256 StateRoot;

  RLP_FIELDS(ExecutionResults, StateRoot)
};

struct Account {
  uint64_t Nonce = 0;
  u256 Balance;
  h256 StorageRootHash;
  h256 CodeHash;
  uint64_t CodeSize = 0;

  RLP_FIELDS(Nonce, Balance, StorageRootHash, CodeHash, CodeSize)

  auto const& storage_root_eth() const { return StorageRootHash ? StorageRootHash : EmptyListSHA3; }
  auto const& code_hash_eth() const { return CodeSize ? CodeHash : EmptySHA3; }
} const ZeroAccount;

struct TrieProof {
  bytes Value;
  vector<bytes> Nodes;

  RLP_FIELDS(Value, Nodes)
};

struct Proof {
  TrieProof AccountProof;
  vector<TrieProof> StorageProofs;

  RLP_FIELDS(AccountProof, StorageProofs)
};

struct Opts {
  uint32_t ExpectedMaxTrxPerBlock = 0;
  uint8_t MainTrieFullNodeLevelsToCache = 0;

  RLP_FIELDS(ExpectedMaxTrxPerBlock, MainTrieFullNodeLevelsToCache)
};

struct OptsDB {
  string db_path;
  bool disable_most_recent_trie_value_views = 0;

  RLP_FIELDS(db_path, disable_most_recent_trie_value_views)
};

struct StateDescriptor {
  BlockNumber blk_num = 0;
  h256 state_root;

  RLP_FIELDS(blk_num, state_root)
};

struct DPOSQuery {
  struct AccountQuery {
    bool with_staking_balance = 0;
    bool with_outbound_deposits = 0;
    bool outbound_deposits_addrs_only = 0;
    bool with_inbound_deposits = 0;
    bool inbound_deposits_addrs_only = 0;

    RLP_FIELDS(with_staking_balance, with_outbound_deposits, outbound_deposits_addrs_only, with_inbound_deposits,
               inbound_deposits_addrs_only)
  };

  bool with_eligible_count = 0;
  unordered_map<addr_t, AccountQuery> account_queries;

  RLP_FIELDS(with_eligible_count, account_queries)
};
void dec_json(Json::Value const& json, DPOSQuery::AccountQuery& obj);
void dec_json(Json::Value const& json, DPOSQuery& obj);

struct DPOSQueryResult {
  struct AccountResult {
    u256 staking_balance;
    bool is_eligible = 0;
    // intentionally used ordered map to have a stable key order in iteration
    using deposits_t = map<addr_t, u256>;
    deposits_t outbound_deposits;
    deposits_t inbound_deposits;

    RLP_FIELDS(staking_balance, is_eligible, outbound_deposits, inbound_deposits)
  };

  uint64_t eligible_count = 0;
  unordered_map<addr_t, AccountResult> account_results;

  RLP_FIELDS(eligible_count, account_results)
};
Json::Value enc_json(DPOSQueryResult::AccountResult const& obj, DPOSQuery::AccountQuery* q = nullptr);
Json::Value enc_json(DPOSQueryResult const& obj, DPOSQuery* q = nullptr);

class StateAPI {
  function<h256(BlockNumber)> get_blk_hash;
  taraxa_evm_GetBlockHash get_blk_hash_c;
  taraxa_evm_state_API_ptr this_c;
  RLPStream rlp_enc_transition_state;
  StateTransitionResult result_buf_transition_state;
  string db_path;

 public:
  StateAPI(function<h256(BlockNumber)> get_blk_hash, ChainConfig const& chain_config, Opts const& opts,
           OptsDB const& opts_db);
  ~StateAPI();

  Proof prove(BlockNumber blk_num, root_t const& state_root, addr_t const& addr, vector<h256> const& keys) const;
  optional<Account> get_account(BlockNumber blk_num, addr_t const& addr) const;
  u256 get_account_storage(BlockNumber blk_num, addr_t const& addr, u256 const& key) const;
  bytes get_code_by_address(BlockNumber blk_num, addr_t const& addr) const;
  ExecutionResult dry_run_transaction(BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
                                      optional<ExecutionOptions> const& opts = nullopt) const;
  StateDescriptor get_last_committed_state_descriptor() const;
  StateTransitionResult const& transition_state(EVMBlock const& block,
                                                RangeView<EVMTransaction> const& transactions,  //
                                                RangeView<UncleBlock> const& uncles = {});
  void transition_state_commit();
  void create_snapshot(uint64_t const& period);
  // DPOS
  uint64_t dpos_eligible_count(BlockNumber blk_num) const;
  uint64_t dpos_eligible_total_vote_count(BlockNumber blk_num) const;
  uint64_t dpos_eligible_vote_count(BlockNumber blk_num, addr_t const& addr) const;

  bool dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const;
  DPOSQueryResult dpos_query(BlockNumber blk_num, DPOSQuery const& q) const;
  static addr_t const& dpos_contract_addr();
  struct DPOSTransactionPrototype {
    uint64_t minimal_gas = 0;  // TODO estimate gas
    byte value = 0;
    bytes input;
    addr_t const& to = dpos_contract_addr();

    DPOSTransactionPrototype(DPOSTransfers const& transfers);
  };
};

}  // namespace taraxa::state_api

namespace taraxa {
using state_api::StateAPI;
}
