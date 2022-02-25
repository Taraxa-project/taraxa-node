#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "config/state_api_config.hpp"

namespace taraxa::state_api {

struct TaraxaEVMError : std::runtime_error {
  std::string const type;
  TaraxaEVMError(std::string type, std::string msg);
  ~TaraxaEVMError() throw();
};

struct ErrFutureBlock : TaraxaEVMError {
  using TaraxaEVMError::TaraxaEVMError;
  ~ErrFutureBlock() throw();
};

struct DPOSTransfer {
  u256 value;
  bool negative = 0;

  HAS_RLP_FIELDS
};

using DPOSTransfers = std::unordered_map<addr_t, DPOSTransfer>;

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
  std::optional<addr_t> to;
  trx_nonce_t nonce = 0;
  val_t value;
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
  std::vector<h256> topics;
  bytes data;

  HAS_RLP_FIELDS
};

struct ExecutionResult {
  bytes code_retval;
  addr_t new_contract_addr;
  std::vector<LogRecord> logs;
  gas_t gas_used = 0;
  std::string code_err;
  std::string consensus_err;

  HAS_RLP_FIELDS
};

struct StateTransitionResult {
  std::vector<ExecutionResult> execution_results;
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
} const ZeroAccount;

struct TrieProof {
  bytes value;
  std::vector<bytes> nodes;

  HAS_RLP_FIELDS
};

struct Proof {
  TrieProof account_proof;
  std::vector<TrieProof> storage_proofs;

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
  std::unordered_map<addr_t, AccountQuery> account_queries;

  HAS_RLP_FIELDS
};
void dec_json(Json::Value const& json, DPOSQuery::AccountQuery& obj);
void dec_json(Json::Value const& json, DPOSQuery& obj);

struct DPOSQueryResult {
  struct AccountResult {
    u256 staking_balance;
    bool is_eligible = 0;
    // intentionally used ordered map to have a stable key order in iteration
    using deposits_t = std::map<addr_t, u256>;
    deposits_t outbound_deposits;
    deposits_t inbound_deposits;

    HAS_RLP_FIELDS
  };

  uint64_t eligible_count = 0;
  std::unordered_map<addr_t, AccountResult> account_results;

  HAS_RLP_FIELDS
};
Json::Value enc_json(DPOSQueryResult::AccountResult const& obj, DPOSQuery::AccountQuery* q = nullptr);
Json::Value enc_json(DPOSQueryResult const& obj, DPOSQuery* q = nullptr);

}  // namespace taraxa::state_api