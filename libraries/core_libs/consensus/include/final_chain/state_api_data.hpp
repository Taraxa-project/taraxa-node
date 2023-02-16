#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "config/state_config.hpp"

namespace taraxa::state_api {

/** @addtogroup FinalChain
 * @{
 */

struct TaraxaEVMError : std::runtime_error {
  std::string const type;
  TaraxaEVMError(std::string&& type, const std::string& msg);
};

struct ErrFutureBlock : TaraxaEVMError {
  using TaraxaEVMError::TaraxaEVMError;
};

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
  u256 total_reward;

  HAS_RLP_FIELDS
};

struct Account {
  trx_nonce_t nonce = 0;
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

struct Tracing {
  bool vmTrace = false;
  bool trace = false;
  bool stateDiff = false;

  HAS_RLP_FIELDS
};

/** @} */
}  // namespace taraxa::state_api