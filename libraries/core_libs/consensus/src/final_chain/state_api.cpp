
#include "final_chain/state_api.hpp"

#include <libdevcore/CommonJS.h>

#include <array>
#include <string_view>

#include "common/encoding_rlp.hpp"

static_assert(sizeof(char) == sizeof(uint8_t));

namespace taraxa::state_api {

bytesConstRef map_bytes(const taraxa_evm_Bytes& b) { return {b.Data, b.Len}; }

taraxa_evm_Bytes map_bytes(const bytes& b) { return {const_cast<uint8_t*>(b.data()), b.size()}; }

template <typename Result>
void from_rlp(taraxa_evm_Bytes b, Result& result) {
  util::rlp(dev::RLP(map_bytes(b), 0), result);
}

void to_str(taraxa_evm_Bytes b, string& result) { result = {reinterpret_cast<char*>(b.Data), b.Len}; }

void to_bytes(taraxa_evm_Bytes b, bytes& result) { result.assign(b.Data, b.Data + b.Len); }

void to_u256(taraxa_evm_Bytes b, u256& result) { result = fromBigEndian<u256>(map_bytes(b)); }

template <typename Result, void (*decode)(taraxa_evm_Bytes, Result&)>
taraxa_evm_BytesCallback decoder_cb_c(Result& res) {
  return {
      &res,
      [](auto receiver, auto b) { decode(b, *static_cast<Result*>(receiver)); },
  };
}

class ErrorHandler {
  std::function<void()> raise_;

 public:
  ErrorHandler() = default;

  taraxa_evm_BytesCallback const cgo_part_{
      this,
      [](auto self, auto err_bytes) {
        auto& raise = decltype(this)(self)->raise_;
        static string const delim = ": ";
        static auto const delim_len = delim.size();
        std::string_view err_str((char*)err_bytes.Data, err_bytes.Len);
        auto delim_pos = err_str.find(delim);
        string type(err_str.substr(0, delim_pos));
        string msg(err_str.substr(delim_pos + delim_len));

        if (type == "github.com/Taraxa-project/taraxa-evm/taraxa/state/state_db/ErrFutureBlock") {
          raise = [err = ErrFutureBlock(std::move(type), msg)] { BOOST_THROW_EXCEPTION(err); };
          return;
        }

        string traceback;
        taraxa_evm_traceback(decoder_cb_c<string, to_str>(traceback));
        msg += "\nGo stack trace:\n" + traceback;
        raise = [err = TaraxaEVMError(std::move(type), msg)] { BOOST_THROW_EXCEPTION(err); };
      },
  };

 public:
  void check() {
    if (raise_) {
      raise_();
    }
  }
};

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, dev::RLPStream& encoding, Result& ret, const Params&... args) {
  util::rlp_tuple(encoding, args...);
  ErrorHandler err_h;
  fn(this_c, map_bytes(encoding.out()), decoder_cb_c<Result, decode>(ret), err_h.cgo_part_);
  err_h.check();
}

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
Result c_method_args_rlp(taraxa_evm_state_API_ptr this_c, const Params&... args) {
  dev::RLPStream encoding;
  Result ret;
  c_method_args_rlp<Result, decode, fn, Params...>(this_c, encoding, ret, args...);
  return ret;
}

template <void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback), typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, const Params&... args) {
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, args...);
  ErrorHandler err_h;
  fn(this_c, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

StateAPI::StateAPI(decltype(get_blk_hash_) get_blk_hash, const Config& state_config, const Opts& opts,
                   const OptsDB& opts_db)
    : get_blk_hash_(std::move(get_blk_hash)),
      get_blk_hash_c_{
          this,
          [](auto receiver, auto arg) {
            const auto& ret = decltype(this)(receiver)->get_blk_hash_(arg);
            taraxa_evm_Hash ret_c;
            std::copy_n(ret.data(), 32, std::begin(ret_c.Val));
            return ret_c;
          },
      },
      db_path_(opts_db.db_path) {
  result_buf_transition_state_.execution_results.reserve(opts.expected_max_trx_per_block);
  rlp_enc_transition_state_.reserve(opts.expected_max_trx_per_block * 1024, opts.expected_max_trx_per_block * 128);
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, reinterpret_cast<uintptr_t>(&get_blk_hash_c_), state_config, opts, opts_db);
  ErrorHandler err_h;
  this_c_ = taraxa_evm_state_api_new(map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

StateAPI::~StateAPI() {
  ErrorHandler err_h;
  taraxa_evm_state_api_free(this_c_, err_h.cgo_part_);
  err_h.check();
}

void StateAPI::update_state_config(const Config& new_config) {
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, new_config);

  ErrorHandler err_h;
  taraxa_evm_state_api_update_state_config(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

std::optional<Account> StateAPI::get_account(EthBlockNumber blk_num, const addr_t& addr) const {
  return c_method_args_rlp<std::optional<Account>, from_rlp, taraxa_evm_state_api_get_account>(this_c_, blk_num, addr);
}

u256 StateAPI::get_account_storage(EthBlockNumber blk_num, const addr_t& addr, const u256& key) const {
  return c_method_args_rlp<u256, to_u256, taraxa_evm_state_api_get_account_storage>(this_c_, blk_num, addr, key);
}

bytes StateAPI::get_code_by_address(EthBlockNumber blk_num, const addr_t& addr) const {
  return c_method_args_rlp<bytes, to_bytes, taraxa_evm_state_api_get_code_by_address>(this_c_, blk_num, addr);
}

ExecutionResult StateAPI::dry_run_transaction(EthBlockNumber blk_num, const EVMBlock& blk,
                                              const EVMTransaction& trx) const {
  return c_method_args_rlp<ExecutionResult, from_rlp, taraxa_evm_state_api_dry_run_transaction>(this_c_, blk_num, blk,
                                                                                                trx);
}

bytes StateAPI::trace(EthBlockNumber blk_num, const EVMBlock& blk, const std::vector<EVMTransaction> trxs,
                      std::optional<Tracing> params) const {
  return c_method_args_rlp<bytes, from_rlp, taraxa_evm_state_api_trace_transactions>(this_c_, blk_num, blk, trxs,
                                                                                     params);
}

StateDescriptor StateAPI::get_last_committed_state_descriptor() const {
  StateDescriptor ret;
  ErrorHandler err_h;
  taraxa_evm_state_api_get_last_committed_state_descriptor(this_c_, decoder_cb_c<StateDescriptor, from_rlp>(ret),
                                                           err_h.cgo_part_);
  err_h.check();
  return ret;
}

const StateTransitionResult& StateAPI::transition_state(const EVMBlock& block,
                                                        const util::RangeView<EVMTransaction>& transactions,
                                                        const std::vector<RewardsStats>& rewards_stats) {
  result_buf_transition_state_.execution_results.clear();
  rlp_enc_transition_state_.clear();
  c_method_args_rlp<StateTransitionResult, from_rlp, taraxa_evm_state_api_transition_state>(
      this_c_, rlp_enc_transition_state_, result_buf_transition_state_, block, transactions, rewards_stats);
  return result_buf_transition_state_;
}

void StateAPI::transition_state_commit() {
  ErrorHandler err_h;
  taraxa_evm_state_api_transition_state_commit(this_c_, err_h.cgo_part_);
  err_h.check();
}

void StateAPI::create_snapshot(PbftPeriod period) {
  auto path = db_path_ + std::to_string(period);
  GoString go_path;
  go_path.p = path.c_str();
  go_path.n = path.size();
  ErrorHandler err_h;
  taraxa_evm_state_api_db_snapshot(this_c_, go_path, 0, err_h.cgo_part_);
  err_h.check();
}

void StateAPI::prune(const std::vector<dev::h256>& state_root_to_keep, EthBlockNumber blk_num) {
  return c_method_args_rlp<taraxa_evm_state_api_prune>(this_c_, state_root_to_keep, blk_num);
}

uint64_t StateAPI::dpos_eligible_total_vote_count(EthBlockNumber blk_num) const {
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_eligible_vote_count(this_c_, blk_num, err_h.cgo_part_);
  err_h.check();
  return ret;
}

uint64_t StateAPI::dpos_eligible_vote_count(EthBlockNumber blk_num, const addr_t& addr) const {
  dev::RLPStream encoding;
  encoding.reserve(sizeof(EthBlockNumber) + sizeof(addr_t) + 8, 1);
  util::rlp_tuple(encoding, blk_num, addr);
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_get_eligible_vote_count(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
  return ret;
}

bool StateAPI::dpos_is_eligible(EthBlockNumber blk_num, const addr_t& addr) const {
  dev::RLPStream encoding;
  encoding.reserve(sizeof(EthBlockNumber) + sizeof(addr_t) + 8, 1);
  util::rlp_tuple(encoding, blk_num, addr);
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_is_eligible(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
  return ret;
}

u256 StateAPI::get_staking_balance(EthBlockNumber blk_num, const addr_t& addr) const {
  return c_method_args_rlp<u256, to_u256, taraxa_evm_state_api_dpos_get_staking_balance>(this_c_, blk_num, addr);
}

vrf_wrapper::vrf_pk_t StateAPI::dpos_get_vrf_key(EthBlockNumber blk_num, const addr_t& addr) const {
  return vrf_wrapper::vrf_pk_t(
      c_method_args_rlp<bytes, to_bytes, taraxa_evm_state_api_dpos_get_vrf_key>(this_c_, blk_num, addr));
}

}  // namespace taraxa::state_api
