#ifndef TARAXA_NODE_STATE_API_STATE_API_HPP_
#define TARAXA_NODE_STATE_API_STATE_API_HPP_

#include <rocksdb/db.h>
#include <taraxa-evm/taraxa-evm.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "db_storage.hpp"
#include "types.hpp"
#include "util/encoding_rlp.hpp"
#include "util/range_view.hpp"

// TODO revert to the C++ naming convention

extern "C" {
struct rocksdb_t {
  rocksdb::DB* rep;
};
struct rocksdb_column_family_handle_t {
  rocksdb::ColumnFamilyHandle* rep;
};
struct rocksdb_writebatch_t {
  rocksdb::WriteBatch rep;
};
}

namespace taraxa::state_api {
using namespace dev;
using namespace eth;
using namespace std;
using namespace util;
using rocksdb::ColumnFamilyHandle;

static constexpr auto BlockNumberNIL = std::numeric_limits<BlockNumber>::max();

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
  bool disable_nonce_check = 0;
  bool disable_gas_fee = 0;
};
void enc_rlp(RLPStream&, ExecutionOptions const&);
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
};
void enc_rlp(RLPStream&, ETHChainConfig const&);
Json::Value enc_json(ETHChainConfig const& obj);
void dec_json(Json::Value const& json, ETHChainConfig& obj);

struct EVMChainConfig {
  ETHChainConfig eth_chain_config;
  ExecutionOptions execution_options;
};
void enc_rlp(RLPStream&, EVMChainConfig const&);
Json::Value enc_json(EVMChainConfig const& obj);
void dec_json(Json::Value const& json, EVMChainConfig& obj);

struct ChainConfig {
  EVMChainConfig evm_chain_config;
  bool disable_block_rewards = 0;
};
void enc_rlp(RLPStream&, ChainConfig const&);
Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

struct EVMBlock {
  addr_t Author;
  gas_t GasLimit = 0;
  uint64_t Time = 0;
  u256 Difficulty;
};
void enc_rlp(RLPStream&, EVMBlock const&);
void dec_rlp(RLP const&, EVMBlock&);

struct EVMTransaction {
  addr_t From;
  u256 GasPrice;
  optional<addr_t> To;
  uint64_t Nonce = 0;
  u256 Value;
  gas_t Gas = 0;
  bytes Input;
};
void enc_rlp(RLPStream&, EVMTransaction const&);
void dec_rlp(RLP const&, EVMTransaction&);

struct EVMTransactionWithHash {
  h256 Hash;
  EVMTransaction Transaction;
};
void enc_rlp(RLPStream&, EVMTransactionWithHash const&);

struct UncleBlock {
  BlockNumber Number = 0;
  addr_t Author;
};
void enc_rlp(RLPStream&, UncleBlock const&);
void dec_rlp(RLP const&, UncleBlock&);

struct ConcurrentSchedule {
  vector<uint32_t> ParallelTransactions;
};
void dec_rlp(RLP const&, ConcurrentSchedule&);
void enc_rlp(RLPStream&, ConcurrentSchedule const&);

struct LogRecord {
  addr_t Address;
  vector<h256> Topics;
  bytes Data;
};
void dec_rlp(RLP const&, LogRecord&);

struct ExecutionResult {
  bytes CodeRet;
  addr_t NewContractAddr;
  vector<LogRecord> Logs;
  gas_t GasUsed = 0;
  string CodeErr;
  string ConsensusErr;
};
void dec_rlp(RLP const&, ExecutionResult&);

struct StateTransitionResult {
  h256 StateRoot;
  vector<ExecutionResult> ExecutionResults;
  unordered_map<addr_t, u256> NonContractBalanceChanges;
};
void dec_rlp(RLP const&, StateTransitionResult&);

struct Account {
  uint64_t Nonce = 0;
  u256 Balance;
  h256 StorageRootHash;
  h256 CodeHash;
  uint64_t CodeSize = 0;

  auto const& storage_root_eth() {
    return StorageRootHash ? StorageRootHash : dev::EmptyListSHA3;
  }
  auto const& code_hash_eth() { return CodeSize ? CodeHash : dev::EmptySHA3; }
};
void dec_rlp(RLP const&, Account&);

struct InputAccount {
  bytes Code;
  std::unordered_map<u256, u256> Storage;
  u256 Balance;
  uint64_t Nonce = 0;
};
using InputAccounts = unordered_map<addr_t, InputAccount>;
void enc_rlp(RLPStream&, InputAccount const&);
void dec_rlp(RLP const&, InputAccount&);

struct TrieProof {
  bytes Value;
  vector<bytes> Nodes;
};
void dec_rlp(RLP const&, TrieProof&);

struct Proof {
  TrieProof AccountProof;
  vector<TrieProof> StorageProofs;
};
void dec_rlp(RLP const&, Proof&);

struct TrieWriterCacheOpts {
  uint8_t FullNodeLevelsToCache = 0;
  uint8_t ExpectedDepth = 0;
};
void enc_rlp(RLPStream&, TrieWriterCacheOpts const&);

struct CacheOpts {
  TrieWriterCacheOpts MainTrieWriterOpts;
  TrieWriterCacheOpts AccTrieWriterOpts;
  uint32_t ExpectedMaxNumTrxPerBlock = 0;
};
void enc_rlp(RLPStream&, CacheOpts const&);

struct Error : std::runtime_error {
  using runtime_error::runtime_error;
};

class StateAPI {
  shared_ptr<rocksdb::DB> db;
  DBColumns cols;
  function<h256(BlockNumber)> get_blk_hash;
  rocksdb_t db_c;
  array<rocksdb_column_family_handle_t, DBColumn::COUNT> cols_c;
  taraxa_evm_GetBlockHash get_blk_hash_c;
  taraxa_evm_state_API_ptr this_c;
  RLPStream StateTransition_ApplyBlock_rlp_strm;
  StateTransitionResult StateTransition_ApplyBlock_ret;

 public:
  StateAPI(shared_ptr<rocksdb::DB> db, DBColumns cols,
           function<h256(BlockNumber)> get_blk_hash,
           ChainConfig const& chain_config, BlockNumber curr_blk_num = 0,
           h256 const& curr_state_root = {}, CacheOpts const& cache_opts = {});

  StateAPI(DbStorage const& db, function<h256(BlockNumber)> get_blk_hash,
           ChainConfig const& chain_config, BlockNumber curr_blk_num = 0,
           h256 const& curr_state_root = {}, CacheOpts const& cache_opts = {});

  ~StateAPI();

  Proof Historical_Prove(BlockNumber blk_num, root_t const& state_root,
                         addr_t const& addr, vector<h256> const& keys) const;
  optional<Account> Historical_GetAccount(BlockNumber blk_num,
                                          addr_t const& addr) const;
  u256 Historical_GetAccountStorage(BlockNumber blk_num, addr_t const& addr,
                                    u256 const& key) const;
  bytes Historical_GetCodeByAddress(BlockNumber blk_num,
                                    addr_t const& addr) const;

  ExecutionResult DryRunner_Apply(
      BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
      optional<ExecutionOptions> const& opts = nullopt) const;

  h256 StateTransition_ApplyAccounts(rocksdb::WriteBatch& batch,
                                     InputAccounts const& accounts);
  StateTransitionResult const& StateTransition_ApplyBlock(
      rocksdb::WriteBatch& batch, EVMBlock const& block,
      RangeView<EVMTransaction> const& transactions,  //
      RangeView<UncleBlock> const& uncles,
      ConcurrentSchedule const& concurrent_schedule);
  void NotifyStateTransitionCommitted();

  void ConcurrentScheduleGeneration_Begin(EVMBlock const& blk);
  void ConcurrentScheduleGeneration_SubmitTransactions(
      RangeView<EVMTransactionWithHash> const& trxs);
  ConcurrentSchedule ConcurrentScheduleGeneration_Commit(
      RangeView<h256> const& trx_hashes);
};

}  // namespace taraxa::state_api

namespace taraxa {
using state_api::StateAPI;
}

#endif  // TARAXA_NODE_STATE_API_STATE_API_HPP_
