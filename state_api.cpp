#include "state_api.hpp"

#include <taraxa-evm/taraxa-evm.h>

#include <string_view>

#include "util/encoding_rlp.hpp"
#include "util/exit_stack.hpp"

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
using util::dec_rlp;
using util::enc_rlp;

string bytes2str(taraxa_evm_Bytes const& b) {
  static_assert(sizeof(char) == sizeof(uint8_t));
  return {(char*)b.Data, b.Len};
}

bytesConstRef map_bytes(taraxa_evm_Bytes const& b) { return {b.Data, b.Len}; }

taraxa_evm_Bytes map_bytes(bytes const& b) {
  return {const_cast<uint8_t*>(b.data()), b.size()};
}

template <typename Result>
void from_rlp(taraxa_evm_Bytes b, Result& result) {
  dec_rlp(RLP(map_bytes(b), 0), result);
}

void to_bytes(taraxa_evm_Bytes b, bytes& result) {
  result.assign(b.Data, b.Data + b.Len);
}

void to_u256(taraxa_evm_Bytes b, u256& result) {
  result = fromBigEndian<u256>(map_bytes(b));
}

void to_h256(taraxa_evm_Bytes b, h256& result) {
  result = h256(b.Data, h256::ConstructFromPointer);
}

taraxa_evm_BytesCallback const err_handler_c{
    nullptr,
    [](auto _, auto err_bytes) {
      BOOST_THROW_EXCEPTION(StateAPI::Error(bytes2str(err_bytes)));
    },
};

struct StateAPIImpl : StateAPI {
  shared_ptr<rocksdb::DB> db;
  DBColumns cols;
  function<h256(BlockNumber)> get_blk_hash;
  rocksdb_t db_c{db.get()};
  array<rocksdb_column_family_handle_t, DBColumn::COUNT> cols_c;
  taraxa_evm_GetBlockHash get_blk_hash_c{
      this,
      [](auto receiver, auto arg) {
        auto const& ret = decltype(this)(receiver)->get_blk_hash(arg);
        taraxa_evm_Hash ret_c;
        copy_n(ret.data(), 32, begin(ret_c.Val));
        return ret_c;
      },
  };
  taraxa_evm_state_API_ptr this_c;

  StateAPIImpl(decltype(db) db, decltype(cols) cols,
               decltype(get_blk_hash) get_blk_hash,
               ChainConfig const& chain_config, BlockNumber curr_blk_num,
               h256 const& curr_state_root, CacheOpts const& cache_opts)
      : db(move(db)), cols(move(cols)), get_blk_hash(move(get_blk_hash)) {
    RLPStream rlp;
    enc_rlp_tuple(
        rlp, &db_c,
        make_range_view(this->cols).map([this](auto const& el, auto i) {
          return &(cols_c[i] = {el.get()});
        }),
        &get_blk_hash_c, chain_config, curr_blk_num, curr_state_root,
        cache_opts);
    this_c = taraxa_evm_state_API_New(map_bytes(rlp.out()), err_handler_c);
  }

  ~StateAPIImpl() { taraxa_evm_state_API_Free(this_c, err_handler_c); }

  template <typename Result,                            //
            void (*decode)(taraxa_evm_Bytes, Result&),  //
            void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes,
                       taraxa_evm_BytesCallback,
                       taraxa_evm_BytesCallback),  //
            typename... Params>
  Result c_method_args_rlp(Params const&... args) {
    RLPStream rlp;
    enc_rlp_tuple(rlp, args...);
    Result ret;
    fn(this_c, map_bytes(rlp.out()),
       {
           &ret,
           [](auto receiver, auto b) { decode(b, *(Result*)receiver); },
       },
       err_handler_c);
    return ret;
  }

  template <void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes,
                       taraxa_evm_BytesCallback),
            typename... Params>
  void c_method_args_rlp(Params const&... args) {
    RLPStream rlp;
    enc_rlp_tuple(rlp, args...);
    fn(this_c, map_bytes(rlp.out()), err_handler_c);
  }

  Proof Historical_Prove(BlockNumber blk_num, root_t const& state_root,
                         addr_t const& addr,
                         vector<h256> const& keys) override {
    return c_method_args_rlp<Proof, from_rlp,
                             taraxa_evm_state_API_Historical_Prove>(
        blk_num, state_root, addr, keys);
  }

  optional<Account> Historical_GetAccount(BlockNumber blk_num,
                                          addr_t const& addr) override {
    return c_method_args_rlp<optional<Account>, from_rlp,
                             taraxa_evm_state_API_Historical_GetAccount>(
        blk_num, addr);
  }

  u256 Historical_GetAccountStorage(BlockNumber blk_num, addr_t const& addr,
                                    u256 const& key) override {
    return c_method_args_rlp<u256, to_u256,
                             taraxa_evm_state_API_Historical_GetAccountStorage>(
        blk_num, addr, key);
  }

  bytes Historical_GetCodeByAddress(BlockNumber blk_num,
                                    addr_t const& addr) override {
    return c_method_args_rlp<bytes, to_bytes,
                             taraxa_evm_state_API_Historical_GetCodeByAddress>(
        blk_num, addr);
  }

  ExecutionResult DryRunner_Apply(
      BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
      optional<ExecutionOptions> const& opts) override {
    return c_method_args_rlp<ExecutionResult, from_rlp,
                             taraxa_evm_state_API_DryRunner_Apply>(blk_num, blk,
                                                                   trx, opts);
  }

  h256 StateTransition_ApplyAccounts(WriteBatch& batch,
                                     InputAccounts const& accounts) override {
    rocksdb_writebatch_t batch_c{move(batch)};
    util::ExitStack exit_stack = [&] { batch = move(batch_c.rep); };
    return c_method_args_rlp<
        h256, to_h256, taraxa_evm_state_API_StateTransition_ApplyAccounts>(
        &batch_c, accounts);
  }

  StateTransitionResult StateTransition_ApplyBlock(
      WriteBatch& batch,
      EVMBlock const& block,  //
      RangeView<EVMTransaction> const& transactions,
      RangeView<UncleBlock> const& uncles,
      ConcurrentSchedule const& concurrent_schedule) override {
    rocksdb_writebatch_t batch_c{move(batch)};
    util::ExitStack exit_stack = [&] { batch = move(batch_c.rep); };
    return c_method_args_rlp<StateTransitionResult, from_rlp,
                             taraxa_evm_state_API_StateTransition_ApplyBlock>(
        &batch_c, block, transactions, uncles, concurrent_schedule);
  }

  void NotifyStateTransitionCommitted() override {
    taraxa_evm_state_API_NotifyStateTransitionCommitted(this_c, err_handler_c);
  }

  void ConcurrentScheduleGeneration_Begin(EVMBlock const& blk) override {
    c_method_args_rlp<taraxa_evm_state_API_ConcurrentScheduleGeneration_Begin>(
        blk);
  }

  void ConcurrentScheduleGeneration_SubmitTransactions(
      RangeView<EVMTransactionWithHash> const& trxs) override {
    c_method_args_rlp<
        taraxa_evm_state_API_ConcurrentScheduleGeneration_SubmitTransactions>(
        trxs);
  }

  ConcurrentSchedule ConcurrentScheduleGeneration_Commit(
      RangeView<h256> const& trx_hashes) override {
    return c_method_args_rlp<
        ConcurrentSchedule, from_rlp,
        taraxa_evm_state_API_ConcurrentScheduleGeneration_Commit>(trx_hashes);
  }
};

unique_ptr<StateAPI> StateAPI::New(shared_ptr<rocksdb::DB> db,
                                   StateAPI::DBColumns cols,
                                   function<h256(BlockNumber)> get_blk_hash,
                                   ChainConfig const& chain_config,
                                   BlockNumber curr_blk_num,
                                   h256 const& curr_state_root,
                                   CacheOpts const& cache_opts) {
  return u_ptr(new StateAPIImpl(move(db), move(cols), move(get_blk_hash),
                                chain_config, curr_blk_num, curr_state_root,
                                cache_opts));
}

unique_ptr<StateAPI> StateAPI::New(DbStorage const& db,
                                   function<h256(BlockNumber)> get_blk_hash,
                                   ChainConfig const& chain_config,
                                   BlockNumber curr_blk_num,
                                   h256 const& curr_state_root,
                                   CacheOpts const& cache_opts) {
  return New(
      db.unwrap(),
      {
          db.unwrap_handle(DbStorage::Columns::eth_state_code),
          db.unwrap_handle(DbStorage::Columns::eth_state_main_trie_node),
          db.unwrap_handle(DbStorage::Columns::eth_state_main_trie_value),
          db.unwrap_handle(
              DbStorage::Columns::eth_state_main_trie_value_latest),
          db.unwrap_handle(DbStorage::Columns::eth_state_acc_trie_node),
          db.unwrap_handle(DbStorage::Columns::eth_state_acc_trie_value),
          db.unwrap_handle(DbStorage::Columns::eth_state_acc_trie_value_latest),
      },
      move(get_blk_hash), chain_config, curr_blk_num, curr_state_root,
      cache_opts);
}

void enc_rlp(RLPStream& rlp, StateAPI::InputAccount const& target) {
  enc_rlp_tuple(rlp, target.Code, target.Storage, target.Balance, target.Nonce);
}

void enc_rlp(RLPStream& rlp, StateAPI::ExecutionOptions const& target) {
  enc_rlp_tuple(rlp, target.DisableNonceCheck, target.DisableGasFee);
}

void enc_rlp(RLPStream& rlp, StateAPI::ETHChainConfig const& target) {
  enc_rlp_tuple(rlp, target.HomesteadBlock, target.DAOForkBlock,
                target.EIP150Block, target.EIP158Block, target.ByzantiumBlock,
                target.ConstantinopleBlock, target.PetersburgBlock);
}

void enc_rlp(RLPStream& rlp, StateAPI::EVMChainConfig const& target) {
  enc_rlp_tuple(rlp, target.ETHChainConfig, target.ExecutionOptions);
}

void enc_rlp(RLPStream& rlp, StateAPI::ChainConfig const& target) {
  enc_rlp_tuple(rlp, target.EVMChainConfig, target.DisableBlockRewards);
}

void enc_rlp(RLPStream& rlp, StateAPI::EVMBlock const& target) {
  enc_rlp_tuple(rlp, target.Author, target.GasLimit, target.Time,
                target.Difficulty);
}

void enc_rlp(RLPStream& rlp, StateAPI::EVMTransaction const& target) {
  enc_rlp_tuple(rlp, target.From, target.GasPrice, target.To, target.Nonce,
                target.Value, target.Gas, target.Input);
}

void enc_rlp(RLPStream& rlp, StateAPI::EVMTransactionWithHash const& target) {
  enc_rlp_tuple(rlp, target.Hash, target.Transaction);
}

void enc_rlp(RLPStream& rlp, StateAPI::UncleBlock const& target) {
  enc_rlp_tuple(rlp, target.Number, target.Author);
}

void enc_rlp(RLPStream& rlp, StateAPI::ConcurrentSchedule const& target) {
  enc_rlp_tuple(rlp, target.ParallelTransactions);
}

void enc_rlp(RLPStream& rlp, StateAPI::TrieWriterCacheOpts const& target) {
  enc_rlp_tuple(rlp, target.FullNodeLevelsToCache, target.ExpectedDepth);
}

void enc_rlp(RLPStream& rlp, StateAPI::CacheOpts const& target) {
  enc_rlp_tuple(rlp, target.MainTrieWriterOpts, target.AccTrieWriterOpts,
                target.ExpectedMaxNumTrxPerBlock);
}

void dec_rlp(RLP const& rlp, StateAPI::Account& target) {
  dec_rlp_tuple(rlp, target.Nonce, target.Balance, target.StorageRootHash,
                target.CodeHash, target.CodeSize);
}

void dec_rlp(RLP const& rlp, StateAPI::LogRecord& target) {
  dec_rlp_tuple(rlp, target.Address, target.Topics, target.Data);
}

void dec_rlp(RLP const& rlp, StateAPI::ExecutionResult& target) {
  dec_rlp_tuple(rlp, target.CodeRet, target.NewContractAddr, target.Logs,
                target.GasUsed, target.CodeErr, target.ConsensusErr);
}

void dec_rlp(RLP const& rlp, StateAPI::StateTransitionResult& target) {
  dec_rlp_tuple(rlp, target.StateRoot, target.ExecutionResults,
                target.BalanceChanges);
}

void dec_rlp(RLP const& rlp, StateAPI::TrieProof& target) {
  dec_rlp_tuple(rlp, target.Value, target.Nodes);
}

void dec_rlp(RLP const& rlp, StateAPI::Proof& target) {
  dec_rlp_tuple(rlp, target.AccountProof, target.StorageProofs);
}

void dec_rlp(RLP const& rlp, StateAPI::ConcurrentSchedule& target) {
  dec_rlp_tuple(rlp, target.ParallelTransactions);
}

void dec_rlp(RLP const& rlp, StateAPI::InputAccount& target) {
  dec_rlp_tuple(rlp, target.Code, target.Storage, target.Balance, target.Nonce);
}

void dec_rlp(RLP const& rlp, StateAPI::UncleBlock& target) {
  dec_rlp_tuple(rlp, target.Number, target.Author);
}

void dec_rlp(RLP const& rlp, StateAPI::EVMBlock& target) {
  dec_rlp_tuple(rlp, target.Author, target.GasLimit, target.Time,
                target.Difficulty);
}

void dec_rlp(RLP const& rlp, StateAPI::EVMTransaction& target) {
  dec_rlp_tuple(rlp, target.From, target.GasPrice, target.To, target.Nonce,
                target.Value, target.Gas, target.Input);
}

}  // namespace taraxa::state_api