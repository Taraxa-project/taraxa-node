#ifndef TARAXA_NODE_STATE_API_STATE_API_HPP_
#define TARAXA_NODE_STATE_API_STATE_API_HPP_

#include <rocksdb/db.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "db_storage.hpp"
#include "types.hpp"
#include "util/range_view.hpp"

namespace taraxa::state_api {
using namespace dev;
using namespace eth;
using namespace std;
using namespace util;
using rocksdb::ColumnFamilyHandle;

struct StateAPI {
  static constexpr auto BlockNumberNIL =
      std::numeric_limits<BlockNumber>::max();

  enum DBColumn {
    code,
    main_trie_node,
    main_trie_value,
    main_trie_value_latest,
    acc_trie_node,
    acc_trie_value,
    acc_trie_value_latest,
    COUNT,
  };
  using DBColumns = array<shared_ptr<ColumnFamilyHandle>, DBColumn::COUNT>;
  struct ExecutionOptions {
    bool DisableNonceCheck = 0;
    bool DisableGasFee = 0;
  };
  struct ETHChainConfig {
    BlockNumber HomesteadBlock = 0;
    BlockNumber DAOForkBlock = 0;
    BlockNumber EIP150Block = 0;
    BlockNumber EIP158Block = 0;
    BlockNumber ByzantiumBlock = 0;
    BlockNumber ConstantinopleBlock = 0;
    BlockNumber PetersburgBlock = 0;
  };
  struct EVMChainConfig {
    ETHChainConfig ETHChainConfig;
    ExecutionOptions ExecutionOptions;
  };
  struct ChainConfig {
    EVMChainConfig EVMChainConfig;
    bool DisableBlockRewards = 0;
  };
  struct EVMBlock {
    addr_t Author;
    gas_t GasLimit = 0;
    uint64_t Time = 0;
    u256 Difficulty = 0;
  };
  struct EVMTransaction {
    addr_t From;
    u256 GasPrice = 0;
    optional<addr_t> To;
    uint64_t Nonce = 0;
    u256 Value = 0;
    gas_t Gas = 0;
    bytes Input;
  };
  struct EVMTransactionWithHash {
    h256 Hash;
    EVMTransaction Transaction;
  };
  struct UncleBlock {
    BlockNumber Number;
    addr_t Author;
  };
  struct ConcurrentSchedule {
    vector<uint32_t> ParallelTransactions;
  };
  struct LogRecord {
    addr_t Address;
    vector<h256> Topics;
    bytes Data;
  };
  struct ExecutionResult {
    bytes CodeRet;
    addr_t NewContractAddr;
    vector<LogRecord> Logs;
    gas_t GasUsed = 0;
    string CodeErr;
    string ConsensusErr;
  };
  struct StateTransitionResult {
    h256 StateRoot;
    vector<ExecutionResult> ExecutionResults;
    unordered_map<addr_t, u256> BalanceChanges;
  };
  struct Account {
    uint64_t Nonce = 0;
    u256 Balance = 0;
    h256 StorageRootHash;
    h256 CodeHash;
    uint64_t CodeSize = 0;
  };
  struct InputAccount {
    bytes Code;
    std::unordered_map<u256, u256> Storage;
    u256 Balance = 0;
    uint64_t Nonce = 0;
  };
  using InputAccounts = unordered_map<addr_t, InputAccount>;
  struct TrieProof {
    bytes Value;
    vector<bytes> Nodes;
  };
  struct Proof {
    TrieProof AccountProof;
    vector<TrieProof> StorageProofs;
  };
  struct TrieWriterCacheOpts {
    uint8_t FullNodeLevelsToCache;
    uint8_t ExpectedDepth;
  };
  struct CacheOpts {
    TrieWriterCacheOpts MainTrieWriterOpts;
    TrieWriterCacheOpts AccTrieWriterOpts;
    uint32_t ExpectedMaxNumTrxPerBlock;
  };
  struct Error : std::runtime_error {
    using runtime_error::runtime_error;
  };

  static unique_ptr<StateAPI> New(shared_ptr<rocksdb::DB> db, DBColumns cols,
                                  function<h256(BlockNumber)> get_blk_hash,
                                  ChainConfig const& chain_config,
                                  BlockNumber curr_blk_num = 0,
                                  h256 const& curr_state_root = {},
                                  CacheOpts const& cache_opts = {});

  static unique_ptr<StateAPI> New(DbStorage const& db,
                                  function<h256(BlockNumber)> get_blk_hash,
                                  ChainConfig const& chain_config,
                                  BlockNumber curr_blk_num = 0,
                                  h256 const& curr_state_root = {},
                                  CacheOpts const& cache_opts = {});

  virtual ~StateAPI(){};

  virtual Proof Historical_Prove(BlockNumber blk_num, root_t const& state_root,
                                 addr_t const& addr,
                                 vector<h256> const& keys) = 0;
  virtual optional<Account> Historical_GetAccount(BlockNumber blk_num,
                                                  addr_t const& addr) = 0;
  virtual u256 Historical_GetAccountStorage(BlockNumber blk_num,
                                            addr_t const& addr,
                                            u256 const& key) = 0;
  virtual bytes Historical_GetCodeByAddress(BlockNumber blk_num,
                                            addr_t const& addr) = 0;

  virtual ExecutionResult DryRunner_Apply(
      BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
      optional<ExecutionOptions> const& opts = nullopt) = 0;

  virtual h256 StateTransition_ApplyAccounts(rocksdb::WriteBatch& batch,
                                             InputAccounts const& accounts) = 0;
  virtual StateTransitionResult StateTransition_ApplyBlock(
      rocksdb::WriteBatch& batch, EVMBlock const& block,
      RangeView<EVMTransaction> const& transactions,  //
      RangeView<UncleBlock> const& uncles,
      ConcurrentSchedule const& concurrent_schedule) = 0;
  virtual void NotifyStateTransitionCommitted() = 0;

  virtual void ConcurrentScheduleGeneration_Begin(EVMBlock const& blk) = 0;
  virtual void ConcurrentScheduleGeneration_SubmitTransactions(
      RangeView<EVMTransactionWithHash> const& trxs) = 0;
  virtual ConcurrentSchedule ConcurrentScheduleGeneration_Commit(
      RangeView<h256> const& trx_hashes) = 0;
};

void enc_rlp(RLPStream&, StateAPI::InputAccount const&);
void enc_rlp(RLPStream&, StateAPI::ExecutionOptions const&);
void enc_rlp(RLPStream&, StateAPI::ETHChainConfig const&);
void enc_rlp(RLPStream&, StateAPI::EVMChainConfig const&);
void enc_rlp(RLPStream&, StateAPI::ChainConfig const&);
void enc_rlp(RLPStream&, StateAPI::EVMBlock const&);
void enc_rlp(RLPStream&, StateAPI::EVMTransaction const&);
void enc_rlp(RLPStream&, StateAPI::EVMTransactionWithHash const&);
void enc_rlp(RLPStream&, StateAPI::UncleBlock const&);
void enc_rlp(RLPStream&, StateAPI::ConcurrentSchedule const&);
void enc_rlp(RLPStream&, StateAPI::TrieWriterCacheOpts const&);
void enc_rlp(RLPStream&, StateAPI::CacheOpts const&);

void dec_rlp(RLP const&, StateAPI::UncleBlock&);
void dec_rlp(RLP const&, StateAPI::EVMBlock&);
void dec_rlp(RLP const&, StateAPI::EVMTransaction&);
void dec_rlp(RLP const&, StateAPI::InputAccount&);
void dec_rlp(RLP const&, StateAPI::Account&);
void dec_rlp(RLP const&, StateAPI::LogRecord&);
void dec_rlp(RLP const&, StateAPI::ExecutionResult&);
void dec_rlp(RLP const&, StateAPI::StateTransitionResult&);
void dec_rlp(RLP const&, StateAPI::TrieProof&);
void dec_rlp(RLP const&, StateAPI::Proof&);
void dec_rlp(RLP const&, StateAPI::ConcurrentSchedule&);

}  // namespace taraxa::state_api

namespace taraxa {
using state_api::StateAPI;
}

#endif  // TARAXA_NODE_STATE_API_STATE_API_HPP_
