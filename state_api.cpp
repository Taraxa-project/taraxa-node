#include "state_api.hpp"

#include <string_view>

#include "util/encoding_rlp.hpp"
#include "util/exit_stack.hpp"

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
      BOOST_THROW_EXCEPTION(Error(bytes2str(err_bytes)));
    },
};

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes,
                     taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, RLPStream& rlp,
                       Result& ret, Params const&... args) {
  enc_rlp_tuple(rlp, args...);
  fn(this_c, map_bytes(rlp.out()),
     {
         &ret,
         [](auto receiver, auto b) { decode(b, *(Result*)receiver); },
     },
     err_handler_c);
}

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes,
                     taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
Result c_method_args_rlp(taraxa_evm_state_API_ptr this_c,
                         Params const&... args) {
  RLPStream rlp;
  Result ret;
  c_method_args_rlp<Result, decode, fn, Params...>(this_c, rlp, ret, args...);
  return ret;
}

template <void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes,
                     taraxa_evm_BytesCallback),
          typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, Params const&... args) {
  RLPStream rlp;
  enc_rlp_tuple(rlp, args...);
  fn(this_c, map_bytes(rlp.out()), err_handler_c);
}

StateAPI::StateAPI(decltype(db) db, decltype(cols) cols,
                   decltype(get_blk_hash) get_blk_hash,
                   ChainConfig const& chain_config, BlockNumber curr_blk_num,
                   h256 const& curr_state_root, CacheOpts const& cache_opts)
    : db(move(db)),
      db_c{this->db.get()},
      cols(move(cols)),
      get_blk_hash(move(get_blk_hash)),
      get_blk_hash_c{
          this,
          [](auto receiver, auto arg) {
            auto const& ret = decltype(this)(receiver)->get_blk_hash(arg);
            taraxa_evm_Hash ret_c;
            copy_n(ret.data(), 32, begin(ret_c.Val));
            return ret_c;
          },
      } {
  StateTransition_ApplyBlock_ret.ExecutionResults.reserve(
      cache_opts.ExpectedMaxNumTrxPerBlock);
  StateTransition_ApplyBlock_ret.NonContractBalanceChanges.reserve(
      cache_opts.ExpectedMaxNumTrxPerBlock * 2);
  StateTransition_ApplyBlock_rlp_strm.reserve(
      cache_opts.ExpectedMaxNumTrxPerBlock * 1024,
      cache_opts.ExpectedMaxNumTrxPerBlock * 128);
  RLPStream rlp;
  enc_rlp_tuple(rlp, &db_c,
                make_range_view(this->cols).map([this](auto const& el, auto i) {
                  return &(cols_c[i] = {el.get()});
                }),
                &get_blk_hash_c, chain_config, curr_blk_num, curr_state_root,
                cache_opts);
  this_c = taraxa_evm_state_API_New(map_bytes(rlp.out()), err_handler_c);
}

StateAPI::StateAPI(DbStorage const& db, decltype(get_blk_hash) get_blk_hash,
                   ChainConfig const& chain_config, BlockNumber curr_blk_num,
                   h256 const& curr_state_root, CacheOpts const& cache_opts)
    : StateAPI(
          db.unwrap(),
          {
              db.unwrap_handle(DbStorage::Columns::eth_state_code),
              db.unwrap_handle(DbStorage::Columns::eth_state_main_trie_node),
              db.unwrap_handle(DbStorage::Columns::eth_state_main_trie_value),
              db.unwrap_handle(
                  DbStorage::Columns::eth_state_main_trie_value_latest),
              db.unwrap_handle(DbStorage::Columns::eth_state_acc_trie_node),
              db.unwrap_handle(DbStorage::Columns::eth_state_acc_trie_value),
              db.unwrap_handle(
                  DbStorage::Columns::eth_state_acc_trie_value_latest),
          },
          move(get_blk_hash), chain_config, curr_blk_num, curr_state_root,
          cache_opts) {}

StateAPI::~StateAPI() { taraxa_evm_state_API_Free(this_c, err_handler_c); }

Proof StateAPI::Historical_Prove(BlockNumber blk_num, root_t const& state_root,
                                 addr_t const& addr,
                                 vector<h256> const& keys) const {
  return c_method_args_rlp<Proof, from_rlp,
                           taraxa_evm_state_API_Historical_Prove>(
      this_c, blk_num, state_root, addr, keys);
}

optional<Account> StateAPI::Historical_GetAccount(BlockNumber blk_num,
                                                  addr_t const& addr) const {
  return c_method_args_rlp<optional<Account>, from_rlp,
                           taraxa_evm_state_API_Historical_GetAccount>(
      this_c, blk_num, addr);
}

u256 StateAPI::Historical_GetAccountStorage(BlockNumber blk_num,
                                            addr_t const& addr,
                                            u256 const& key) const {
  return c_method_args_rlp<u256, to_u256,
                           taraxa_evm_state_API_Historical_GetAccountStorage>(
      this_c, blk_num, addr, key);
}

bytes StateAPI::Historical_GetCodeByAddress(BlockNumber blk_num,
                                            addr_t const& addr) const {
  return c_method_args_rlp<bytes, to_bytes,
                           taraxa_evm_state_API_Historical_GetCodeByAddress>(
      this_c, blk_num, addr);
}

ExecutionResult StateAPI::DryRunner_Apply(
    BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
    optional<ExecutionOptions> const& opts) const {
  return c_method_args_rlp<ExecutionResult, from_rlp,
                           taraxa_evm_state_API_DryRunner_Apply>(
      this_c, blk_num, blk, trx, opts);
}

h256 StateAPI::StateTransition_ApplyAccounts(WriteBatch& batch,
                                             InputAccounts const& accounts) {
  rocksdb_writebatch_t batch_c{move(batch)};
  util::ExitStack exit_stack = [&] { batch = move(batch_c.rep); };
  return c_method_args_rlp<h256, to_h256,
                           taraxa_evm_state_API_StateTransition_ApplyAccounts>(
      this_c, &batch_c, accounts);
}

StateTransitionResult const& StateAPI::StateTransition_ApplyBlock(
    WriteBatch& batch,
    EVMBlock const& block,  //
    RangeView<EVMTransaction> const& transactions,
    RangeView<UncleBlock> const& uncles,
    ConcurrentSchedule const& concurrent_schedule) {
  rocksdb_writebatch_t batch_c{move(batch)};
  util::ExitStack exit_stack = [&] { batch = move(batch_c.rep); };
  StateTransition_ApplyBlock_ret.ExecutionResults.clear();
  StateTransition_ApplyBlock_ret.NonContractBalanceChanges.clear();
  StateTransition_ApplyBlock_rlp_strm.clear();
  c_method_args_rlp<StateTransitionResult, from_rlp,
                    taraxa_evm_state_API_StateTransition_ApplyBlock>(
      this_c, StateTransition_ApplyBlock_rlp_strm,
      StateTransition_ApplyBlock_ret, &batch_c, block, transactions, uncles,
      concurrent_schedule);
  return StateTransition_ApplyBlock_ret;
}

void StateAPI::NotifyStateTransitionCommitted() {
  taraxa_evm_state_API_NotifyStateTransitionCommitted(this_c, err_handler_c);
}

void StateAPI::ConcurrentScheduleGeneration_Begin(EVMBlock const& blk) {
  c_method_args_rlp<taraxa_evm_state_API_ConcurrentScheduleGeneration_Begin>(
      this_c, blk);
}

void StateAPI::ConcurrentScheduleGeneration_SubmitTransactions(
    RangeView<EVMTransactionWithHash> const& trxs) {
  c_method_args_rlp<
      taraxa_evm_state_API_ConcurrentScheduleGeneration_SubmitTransactions>(
      this_c, trxs);
}

ConcurrentSchedule StateAPI::ConcurrentScheduleGeneration_Commit(
    RangeView<h256> const& trx_hashes) {
  return c_method_args_rlp<
      ConcurrentSchedule, from_rlp,
      taraxa_evm_state_API_ConcurrentScheduleGeneration_Commit>(this_c,
                                                                trx_hashes);
}

void enc_rlp(RLPStream& rlp, InputAccount const& target) {
  enc_rlp_tuple(rlp, target.Code, target.Storage, target.Balance, target.Nonce);
}

void enc_rlp(RLPStream& rlp, ExecutionOptions const& target) {
  enc_rlp_tuple(rlp, target.DisableNonceCheck, target.DisableGasFee);
}

void enc_rlp(RLPStream& rlp, ETHChainConfig const& target) {
  enc_rlp_tuple(rlp, target.HomesteadBlock, target.DAOForkBlock,
                target.EIP150Block, target.EIP158Block, target.ByzantiumBlock,
                target.ConstantinopleBlock, target.PetersburgBlock);
}

void enc_rlp(RLPStream& rlp, EVMChainConfig const& target) {
  enc_rlp_tuple(rlp, target.eth_chain_config, target.execution_options);
}

void enc_rlp(RLPStream& rlp, ChainConfig const& target) {
  enc_rlp_tuple(rlp, target.evm_chain_config, target.disable_block_rewards);
}

void enc_rlp(RLPStream& rlp, EVMBlock const& target) {
  enc_rlp_tuple(rlp, target.Author, target.GasLimit, target.Time,
                target.Difficulty);
}

void enc_rlp(RLPStream& rlp, EVMTransaction const& target) {
  enc_rlp_tuple(rlp, target.From, target.GasPrice,
                target.To ? target.To->ref() : bytesConstRef(), target.Nonce,
                target.Value, target.Gas, target.Input);
}

void enc_rlp(RLPStream& rlp, EVMTransactionWithHash const& target) {
  enc_rlp_tuple(rlp, target.Hash, target.Transaction);
}

void enc_rlp(RLPStream& rlp, UncleBlock const& target) {
  enc_rlp_tuple(rlp, target.Number, target.Author);
}

void enc_rlp(RLPStream& rlp, ConcurrentSchedule const& target) {
  enc_rlp_tuple(rlp, target.ParallelTransactions);
}

void enc_rlp(RLPStream& rlp, TrieWriterCacheOpts const& target) {
  enc_rlp_tuple(rlp, target.FullNodeLevelsToCache, target.ExpectedDepth);
}

void enc_rlp(RLPStream& rlp, CacheOpts const& target) {
  enc_rlp_tuple(rlp, target.MainTrieWriterOpts, target.AccTrieWriterOpts,
                target.ExpectedMaxNumTrxPerBlock);
}

void dec_rlp(RLP const& rlp, Account& target) {
  dec_rlp_tuple(rlp, target.Nonce, target.Balance, target.StorageRootHash,
                target.CodeHash, target.CodeSize);
}

void dec_rlp(RLP const& rlp, LogRecord& target) {
  dec_rlp_tuple(rlp, target.Address, target.Topics, target.Data);
}

void dec_rlp(RLP const& rlp, ExecutionResult& target) {
  dec_rlp_tuple(rlp, target.CodeRet, target.NewContractAddr, target.Logs,
                target.GasUsed, target.CodeErr, target.ConsensusErr);
}

void dec_rlp(RLP const& rlp, StateTransitionResult& target) {
  dec_rlp_tuple(rlp, target.StateRoot, target.ExecutionResults,
                target.NonContractBalanceChanges);
}

void dec_rlp(RLP const& rlp, TrieProof& target) {
  dec_rlp_tuple(rlp, target.Value, target.Nodes);
}

void dec_rlp(RLP const& rlp, Proof& target) {
  dec_rlp_tuple(rlp, target.AccountProof, target.StorageProofs);
}

void dec_rlp(RLP const& rlp, ConcurrentSchedule& target) {
  dec_rlp_tuple(rlp, target.ParallelTransactions);
}

void dec_rlp(RLP const& rlp, InputAccount& target) {
  dec_rlp_tuple(rlp, target.Code, target.Storage, target.Balance, target.Nonce);
}

void dec_rlp(RLP const& rlp, UncleBlock& target) {
  dec_rlp_tuple(rlp, target.Number, target.Author);
}

void dec_rlp(RLP const& rlp, EVMBlock& target) {
  dec_rlp_tuple(rlp, target.Author, target.GasLimit, target.Time,
                target.Difficulty);
}

void dec_rlp(RLP const& rlp, EVMTransaction& target) {
  dec_rlp_tuple(rlp, target.From, target.GasPrice, target.To, target.Nonce,
                target.Value, target.Gas, target.Input);
}

}  // namespace taraxa::state_api