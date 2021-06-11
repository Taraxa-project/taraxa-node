#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/types.hpp"
#include "util/encoding_rlp.hpp"

namespace taraxa::state_api {
using namespace ::dev;
using namespace ::std;
using namespace ::taraxa::util;

static constexpr auto BlockNumberNIL = std::numeric_limits<EthBlockNumber>::max();

struct TaraxaEVMError : std::runtime_error {
  string const type;
  TaraxaEVMError(string type, string msg);
  ~TaraxaEVMError() throw();
};

struct ErrFutureBlock : TaraxaEVMError {
  using TaraxaEVMError::TaraxaEVMError;
  ~ErrFutureBlock() throw();
};

struct ExecutionOptions {
  bool disable_nonce_check = 0;
  bool disable_gas_fee = 0;

  HAS_RLP_FIELDS
};
Json::Value enc_json(ExecutionOptions const& obj);
void dec_json(Json::Value const& json, ExecutionOptions& obj);

struct ETHChainConfig {
  EthBlockNumber homestead_block = 0;
  EthBlockNumber dao_fork_block = 0;
  EthBlockNumber eip_150_block = 0;
  EthBlockNumber eip_158_block = 0;
  EthBlockNumber byzantium_block = 0;
  EthBlockNumber constantinople_block = 0;
  EthBlockNumber petersburg_block = 0;

  HAS_RLP_FIELDS
};
Json::Value enc_json(ETHChainConfig const& obj);
void dec_json(Json::Value const& json, ETHChainConfig& obj);

using BalanceMap = unordered_map<addr_t, u256>;
Json::Value enc_json(BalanceMap const& obj);
void dec_json(Json::Value const& json, BalanceMap& obj);

struct DPOSConfig {
  u256 eligibility_balance_threshold;
  EthBlockNumber deposit_delay = 0;
  EthBlockNumber withdrawal_delay = 0;
  unordered_map<addr_t, BalanceMap> genesis_state;

  HAS_RLP_FIELDS
};
Json::Value enc_json(DPOSConfig const& obj);
void dec_json(Json::Value const& json, DPOSConfig& obj);

struct DPOSTransfer {
  u256 value;
  bool negative = 0;

  HAS_RLP_FIELDS
};

using DPOSTransfers = unordered_map<addr_t, DPOSTransfer>;

struct Config {
  ETHChainConfig eth_chain_config;
  bool disable_block_rewards = 0;
  ExecutionOptions execution_options;
  BalanceMap genesis_balances;
  optional<DPOSConfig> dpos;

  HAS_RLP_FIELDS

  u256 effective_genesis_balance(addr_t const& addr) const;
};
Json::Value enc_json(Config const& obj);
void dec_json(Json::Value const& json, Config& obj);

struct EVMBlock {
  addr_t author;
  gas_t gas_limit = 0;
  uint64_t time = 0;
  u256 difficulty;

  HAS_RLP_FIELDS
};

struct EVMTransaction {
  addr_t from;
  u256 gas_price;
  optional<addr_t> to;
  uint64_t nonce = 0;
  u256 value;
  gas_t gas = 0;
  bytes input;

  HAS_RLP_FIELDS
};

struct UncleBlock {
  EthBlockNumber number = 0;
  addr_t author;

  HAS_RLP_FIELDS
};

struct LogRecord {
  addr_t address;
  vector<h256> topics;
  bytes data;

  HAS_RLP_FIELDS
};

struct ExecutionResult {
  bytes code_retval;
  addr_t new_contract_addr;
  vector<LogRecord> logs;
  gas_t gas_used = 0;
  string code_err;
  string consensus_err;

  HAS_RLP_FIELDS
};

struct StateTransitionResult {
  vector<ExecutionResult> execution_results;
  h256 state_root;

  HAS_RLP_FIELDS
};

struct Account {
  uint64_t nonce = 0;
  u256 balance;
  h256 storage_root_hash;
  h256 code_hash;
  uint64_t code_size = 0;

  HAS_RLP_FIELDS

  h256 const& storage_root_eth() const;
  h256 const& code_hash_eth() const;
} const ZeroAccount;

struct TrieProof {
  bytes value;
  vector<bytes> nodes;

  HAS_RLP_FIELDS
};

struct Proof {
  TrieProof account_proof;
  vector<TrieProof> storage_proofs;

  HAS_RLP_FIELDS
};

struct Opts {
  uint32_t expected_max_trx_per_block = 0;
  uint8_t max_trie_full_node_levels_to_cache = 0;

  HAS_RLP_FIELDS
};

struct OptsDB {
  string db_path;
  bool disable_most_recent_trie_value_views = 0;

  HAS_RLP_FIELDS
};

struct StateDescriptor {
  EthBlockNumber blk_num = 0;
  h256 state_root;

  HAS_RLP_FIELDS
};

struct DPOSQuery {
  struct AccountQuery {
    bool with_staking_balance = 0;
    bool with_outbound_deposits = 0;
    bool outbound_deposits_addrs_only = 0;
    bool with_inbound_deposits = 0;
    bool inbound_deposits_addrs_only = 0;

    HAS_RLP_FIELDS
  };

  bool with_eligible_count = 0;
  unordered_map<addr_t, AccountQuery> account_queries;

  HAS_RLP_FIELDS
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

    HAS_RLP_FIELDS
  };

  uint64_t eligible_count = 0;
  unordered_map<addr_t, AccountResult> account_results;

  HAS_RLP_FIELDS
};
Json::Value enc_json(DPOSQueryResult::AccountResult const& obj, DPOSQuery::AccountQuery* q = nullptr);
Json::Value enc_json(DPOSQueryResult const& obj, DPOSQuery* q = nullptr);

}  // namespace taraxa::state_api