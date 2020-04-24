#ifndef TARAXA_NODE_TARAXA_EVM_CLIENT_CLIENT_HPP_
#define TARAXA_NODE_TARAXA_EVM_CLIENT_CLIENT_HPP_

#include <rocksdb/db.h>
#include <taraxa-evm/taraxa-evm.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../types.hpp"
#include "common.hpp"
#include "eth/util.hpp"

namespace taraxa::taraxa_evm::state_api {
using dev::h256;
using dev::u256;
using rocksdb::DB;
using rocksdb::WriteBatch;
using std::function;
using std::move;
using std::nullopt;
using std::optional;
using std::string;
using std::unique_ptr;
using std::vector;

struct StateAPI {
  struct DBColumns {
    using COL = rocksdb::ColumnFamilyHandle *;
    COL COL_code;
    COL COL_main_trie_node;
    COL COL_main_trie_value;
    COL COL_main_trie_value_latest;
    COL COL_acc_trie_node;
    COL COL_acc_trie_value;
    COL COL_acc_trie_value_latest;
  };
  struct ExecutionOptions {
    bool DisableNonceCheck;
    bool DisableGasFee;
  };
  struct ETHChainConfig {
    ETHBlockNum HomesteadBlock;
    ETHBlockNum DAOForkBlock;
    ETHBlockNum EIP150Block;
    ETHBlockNum EIP158Block;
    ETHBlockNum ByzantiumBlock;
    ETHBlockNum ConstantinopleBlock;
    ETHBlockNum PetersburgBlock;
  };
  struct EVMChainConfig {
    ETHChainConfig ETHChainConfig;
    ExecutionOptions ExecutionOptions;
  };
  struct ChainConfig {
    EVMChainConfig EVMChainConfig;
    bool DisableBlockRewards;
  };
  struct EVMBlock {
    ETHBlockNum Number;
    Address Author;
    gas_t GasLimit;
    uint64_t Time;
    u256 Difficulty;
  };
  struct EVMTransaction {
    addr_t From;
    u256 GasPrice;
    optional<addr_t> To;
    uint64_t Nonce;
    u256 Value;
    gas_t Gas;
    string Input;
  };
  struct EVMTransactionWithHash {
    h256 Hash;
    EVMTransaction Transaction;
  };
  struct UncleBlock {
    ETHBlockNum Number;
    addr_t Author;
  };
  struct ConcurrentSchedule {
    vector<uint32_t> ParallelTransactions;
  };
  struct StateTransitionParams {
    EVMBlock Block;
    vector<UncleBlock> Uncles;
    vector<EVMTransaction> Transactions;
    ConcurrentSchedule ConcurrentSchedule;
  };
  struct LogRecord {
    addr_t Address;
    vector<h256> Topics;
    string Data;
  };
  struct ExecutionResult {
    string CodeRet;
    addr_t NewContractAddr;
    vector<LogRecord> Logs;
    gas_t GasUsed;
    string CodeErr;
    string ConsensusErr;
  };
  struct StateTransitionResult {
    h256 StateRoot;
    vector<ExecutionResult> ExecutionResults;
  };
  struct Account {
    uint64_t Nonce;
    u256 Balance;
    h256 StorageRootHash;
    h256 CodeHash;
    uint64_t CodeSize;
  };
  struct GenesisAccount {
    uint64_t Nonce;
    u256 Balance;
    unordered_map<u256, u256> Storage;
    string Code;
  };
  struct TrieProof {
    string Value;
    vector<string> Nodes;
  };
  struct Proof {
    TrieProof AccountProof;
    vector<TrieProof> StorageProofs;
  };

  static unique_ptr<StateAPI> DefaultImpl(
      DB *db, DBColumns const &cols, function<h256(ETHBlockNum)> get_blk_hash,
      h256 const &last_state_root, ChainConfig const &chain_config);

  virtual ~StateAPI() = default;

  virtual void DB_Refresh() = 0;
  virtual Proof Historical_Prove(ETHBlockNum blk_num, root_t const &state_root,
                                 addr_t const &addr,
                                 vector<h256> const &keys) = 0;
  virtual Account Historical_GetAccount(ETHBlockNum blk_num,
                                        addr_t const &addr) = 0;
  virtual u256 Historical_GetAccountStorage(ETHBlockNum blk_num,
                                            addr_t const &addr,
                                            u256 const &key) = 0;
  virtual string Historical_GetCodeByAddress(ETHBlockNum blk_num,
                                             addr_t const &addr) = 0;
  virtual ExecutionResult DryRunner_Apply(
      EVMBlock const &blk, EVMTransaction const &trx,
      optional<ExecutionOptions> const &opts = nullopt) = 0;
  virtual h256 StateTransition_ApplyGenesis(
      rocksdb::WriteBatch &batch,
      unordered_map<addr_t, GenesisAccount> const &accounts) = 0;
  virtual StateTransitionResult StateTransition_Apply(
      rocksdb::WriteBatch &batch, StateTransitionParams const &params) = 0;
  virtual void ConcurrentScheduleGeneration_Begin(EVMBlock const &blk) = 0;
  virtual void ConcurrentScheduleGeneration_SubmitTransactions(
      vector<EVMTransactionWithHash> const &trxs) = 0;
  virtual ConcurrentSchedule ConcurrentScheduleGeneration_Commit(
      vector<h256> const &trx_hashes) = 0;
};

}  // namespace taraxa::taraxa_evm::state_api

#endif  // TARAXA_NODE_TARAXA_EVM_CLIENT_CLIENT_HPP_
