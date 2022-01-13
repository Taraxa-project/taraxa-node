
#include "final_chain/state_api.hpp"

#include <libdevcore/CommonJS.h>

#include <array>
#include <string_view>

#include "common/encoding_rlp.hpp"

static_assert(sizeof(char) == sizeof(uint8_t));

namespace taraxa::state_api {

bytesConstRef map_bytes(taraxa_evm_Bytes const& b) { return {b.Data, b.Len}; }

taraxa_evm_Bytes map_bytes(bytes const& b) { return {const_cast<uint8_t*>(b.data()), b.size()}; }

template <typename Result>
void from_rlp(taraxa_evm_Bytes b, Result& result) {
  util::rlp(dev::RLP(map_bytes(b), 0), result);
}

void to_str(taraxa_evm_Bytes b, string& result) { result = {(char*)b.Data, b.Len}; }

void to_bytes(taraxa_evm_Bytes b, bytes& result) { result.assign(b.Data, b.Data + b.Len); }

void to_u256(taraxa_evm_Bytes b, u256& result) { result = fromBigEndian<u256>(map_bytes(b)); }

template <typename Result, void (*decode)(taraxa_evm_Bytes, Result&)>
taraxa_evm_BytesCallback decoder_cb_c(Result& res) {
  return {
      &res,
      [](auto receiver, auto b) { decode(b, *(Result*)receiver); },
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
          raise = [err = ErrFutureBlock(move(type), move(msg))] { BOOST_THROW_EXCEPTION(err); };
          return;
        }

        string traceback;
        taraxa_evm_traceback(decoder_cb_c<string, to_str>(traceback));
        msg += "\nGo stack trace:\n" + traceback;
        raise = [err = TaraxaEVMError(move(type), move(msg))] { BOOST_THROW_EXCEPTION(err); };
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
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, dev::RLPStream& encoding, Result& ret, Params const&... args) {
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
Result c_method_args_rlp(taraxa_evm_state_API_ptr this_c, Params const&... args) {
  dev::RLPStream encoding;
  Result ret;
  c_method_args_rlp<Result, decode, fn, Params...>(this_c, encoding, ret, args...);
  return ret;
}

template <void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback), typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, Params const&... args) {
  dev::RLPStream encoding;
  rlp_tuple(encoding, args...);
  ErrorHandler err_h;
  fn(this_c, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

StateAPI::StateAPI(decltype(get_blk_hash_) get_blk_hash, Config const& chain_config, Opts const& opts,
                   OptsDB const& opts_db)
    : get_blk_hash_(move(get_blk_hash)),
      get_blk_hash_c_{
          this,
          [](auto receiver, auto arg) {
            auto const& ret = decltype(this)(receiver)->get_blk_hash_(arg);
            taraxa_evm_Hash ret_c;
            std::copy_n(ret.data(), 32, std::begin(ret_c.Val));
            return ret_c;
          },
      },
      db_path_(opts_db.db_path) {
  result_buf_transition_state_.execution_results.reserve(opts.expected_max_trx_per_block);
  rlp_enc_transition_state_.reserve(opts.expected_max_trx_per_block * 1024, opts.expected_max_trx_per_block * 128);
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, reinterpret_cast<uintptr_t>(&get_blk_hash_c_), chain_config, opts, opts_db);
  ErrorHandler err_h;
  this_c_ = taraxa_evm_state_api_new(map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

StateAPI::~StateAPI() {
  ErrorHandler err_h;
  taraxa_evm_state_api_free(this_c_, err_h.cgo_part_);
  err_h.check();
}

void StateAPI::update_state_config(const Config& new_config) const {
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, new_config);

  ErrorHandler err_h;
  taraxa_evm_state_api_update_state_config(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
}

Proof StateAPI::prove(EthBlockNumber blk_num, root_t const& state_root, addr_t const& addr,
                      std::vector<h256> const& keys) const {
  return c_method_args_rlp<Proof, from_rlp, taraxa_evm_state_api_prove>(this_c_, blk_num, state_root, addr, keys);
}

std::optional<Account> StateAPI::get_account(EthBlockNumber blk_num, addr_t const& addr) const {
  return c_method_args_rlp<std::optional<Account>, from_rlp, taraxa_evm_state_api_get_account>(this_c_, blk_num, addr);
}

u256 StateAPI::get_account_storage(EthBlockNumber blk_num, addr_t const& addr, u256 const& key) const {
  return c_method_args_rlp<u256, to_u256, taraxa_evm_state_api_get_account_storage>(this_c_, blk_num, addr, key);
}

bytes StateAPI::get_code_by_address(EthBlockNumber blk_num, addr_t const& addr) const {
  return c_method_args_rlp<bytes, to_bytes, taraxa_evm_state_api_get_code_by_address>(this_c_, blk_num, addr);
}

ExecutionResult StateAPI::dry_run_transaction(EthBlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
                                              std::optional<ExecutionOptions> const& opts) const {
  return c_method_args_rlp<ExecutionResult, from_rlp, taraxa_evm_state_api_dry_run_transaction>(this_c_, blk_num, blk,
                                                                                                trx, opts);
}

StateDescriptor StateAPI::get_last_committed_state_descriptor() const {
  StateDescriptor ret;
  ErrorHandler err_h;
  taraxa_evm_state_api_get_last_committed_state_descriptor(this_c_, decoder_cb_c<StateDescriptor, from_rlp>(ret),
                                                           err_h.cgo_part_);
  err_h.check();
  return ret;
}

StateTransitionResult const& StateAPI::transition_state(EVMBlock const& block,  //
                                                        util::RangeView<EVMTransaction> const& transactions,
                                                        util::RangeView<UncleBlock> const& uncles) {
  result_buf_transition_state_.execution_results.clear();
  rlp_enc_transition_state_.clear();
  c_method_args_rlp<StateTransitionResult, from_rlp, taraxa_evm_state_api_transition_state>(
      this_c_, rlp_enc_transition_state_, result_buf_transition_state_, block, transactions, uncles);
  return result_buf_transition_state_;
}

void StateAPI::transition_state_commit() {
  ErrorHandler err_h;
  taraxa_evm_state_api_transition_state_commit(this_c_, err_h.cgo_part_);
  err_h.check();
}

void StateAPI::create_snapshot(uint64_t period) {
  auto path = db_path_ + std::to_string(period);
  GoString go_path;
  go_path.p = path.c_str();
  go_path.n = path.size();
  ErrorHandler err_h;
  taraxa_evm_state_api_db_snapshot(this_c_, go_path, 0, err_h.cgo_part_);
  err_h.check();
}

uint64_t StateAPI::dpos_eligible_count(EthBlockNumber blk_num) const {
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_eligible_count(this_c_, blk_num, err_h.cgo_part_);
  err_h.check();
  return ret;
}

uint64_t StateAPI::dpos_eligible_total_vote_count(EthBlockNumber blk_num) const {
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_eligible_vote_count(this_c_, blk_num, err_h.cgo_part_);
  err_h.check();
  return ret;
}

uint64_t StateAPI::dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const {
  dev::RLPStream encoding;
  encoding.reserve(sizeof(EthBlockNumber) + sizeof(addr_t) + 8, 1);
  util::rlp_tuple(encoding, blk_num, addr);
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_get_eligible_vote_count(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
  return ret;
}

bool StateAPI::dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const {
  dev::RLPStream encoding;
  encoding.reserve(sizeof(EthBlockNumber) + sizeof(addr_t) + 8, 1);
  util::rlp_tuple(encoding, blk_num, addr);
  ErrorHandler err_h;
  auto ret = taraxa_evm_state_api_dpos_is_eligible(this_c_, map_bytes(encoding.out()), err_h.cgo_part_);
  err_h.check();
  return ret;
}

DPOSQueryResult StateAPI::dpos_query(EthBlockNumber blk_num, DPOSQuery const& q) const {
  return c_method_args_rlp<DPOSQueryResult, from_rlp, taraxa_evm_state_api_dpos_query>(this_c_, blk_num, q);
}

addr_t const& StateAPI::dpos_contract_addr() {
  static auto const ret = [] {
    auto ret_c = taraxa_evm_state_api_dpos_contract_addr();
    return addr_t(ret_c.Val, addr_t::ConstructFromPointer);
  }();
  return ret;
}

StateAPI::DPOSTransactionPrototype::DPOSTransactionPrototype(DPOSTransfers const& transfers) {
  dev::RLPStream transfers_rlp;
  util::rlp(transfers_rlp, transfers);
  input = move(transfers_rlp.invalidate());
}

}  // namespace taraxa::state_api