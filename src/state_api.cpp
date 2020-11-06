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

taraxa_evm_Bytes map_bytes(bytes const& b) { return {const_cast<uint8_t*>(b.data()), b.size()}; }

template <typename Result>
void from_rlp(taraxa_evm_Bytes b, Result& result) {
  dec_rlp(RLP(map_bytes(b), 0), result);
}

void to_bytes(taraxa_evm_Bytes b, bytes& result) { result.assign(b.Data, b.Data + b.Len); }

void to_u256(taraxa_evm_Bytes b, u256& result) { result = fromBigEndian<u256>(map_bytes(b)); }

void to_h256(taraxa_evm_Bytes b, h256& result) { result = h256(b.Data, h256::ConstructFromPointer); }

taraxa_evm_BytesCallback const err_handler_c{
    nullptr,
    [](auto _, auto err_bytes) { BOOST_THROW_EXCEPTION(Error(bytes2str(err_bytes))); },
};

template <typename Result, void (*decode)(taraxa_evm_Bytes, Result&)>
taraxa_evm_BytesCallback decoder_cb_c(Result& res) {
  return {
      &res,
      [](auto receiver, auto b) { decode(b, *(Result*)receiver); },
  };
}

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, RLPStream& rlp, Result& ret, Params const&... args) {
  enc_rlp_tuple(rlp, args...);
  fn(this_c, map_bytes(rlp.out()), decoder_cb_c<Result, decode>(ret), err_handler_c);
}

template <typename Result,                            //
          void (*decode)(taraxa_evm_Bytes, Result&),  //
          void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback,
                     taraxa_evm_BytesCallback),  //
          typename... Params>
Result c_method_args_rlp(taraxa_evm_state_API_ptr this_c, Params const&... args) {
  RLPStream rlp;
  Result ret;
  c_method_args_rlp<Result, decode, fn, Params...>(this_c, rlp, ret, args...);
  return ret;
}

template <void (*fn)(taraxa_evm_state_API_ptr, taraxa_evm_Bytes, taraxa_evm_BytesCallback), typename... Params>
void c_method_args_rlp(taraxa_evm_state_API_ptr this_c, Params const&... args) {
  RLPStream rlp;
  enc_rlp_tuple(rlp, args...);
  fn(this_c, map_bytes(rlp.out()), err_handler_c);
}

StateAPI::StateAPI(string const& db_path, decltype(get_blk_hash) get_blk_hash, ChainConfig const& chain_config, Opts const& opts)
    : get_blk_hash(move(get_blk_hash)),
      get_blk_hash_c{
          this,
          [](auto receiver, auto arg) {
            auto const& ret = decltype(this)(receiver)->get_blk_hash(arg);
            taraxa_evm_Hash ret_c;
            copy_n(ret.data(), 32, begin(ret_c.Val));
            return ret_c;
          },
      },
      db_path(db_path) {
  result_buf_transition_state.ExecutionResults.reserve(opts.ExpectedMaxTrxPerBlock);
  rlp_enc_transition_state.reserve(opts.ExpectedMaxTrxPerBlock * 1024, opts.ExpectedMaxTrxPerBlock * 128);
  RLPStream rlp;
  enc_rlp_tuple(rlp, db_path, &get_blk_hash_c, chain_config, opts);
  this_c = taraxa_evm_state_api_new(map_bytes(rlp.out()), err_handler_c);
}

StateAPI::~StateAPI() { taraxa_evm_state_api_free(this_c, err_handler_c); }

Proof StateAPI::prove(BlockNumber blk_num, root_t const& state_root, addr_t const& addr, vector<h256> const& keys) const {
  return c_method_args_rlp<Proof, from_rlp, taraxa_evm_state_api_prove>(this_c, blk_num, state_root, addr, keys);
}

optional<Account> StateAPI::get_account(BlockNumber blk_num, addr_t const& addr) const {
  return c_method_args_rlp<optional<Account>, from_rlp, taraxa_evm_state_api_get_account>(this_c, blk_num, addr);
}

u256 StateAPI::get_account_storage(BlockNumber blk_num, addr_t const& addr, u256 const& key) const {
  return c_method_args_rlp<u256, to_u256, taraxa_evm_state_api_get_account_storage>(this_c, blk_num, addr, key);
}

bytes StateAPI::get_code_by_address(BlockNumber blk_num, addr_t const& addr) const {
  return c_method_args_rlp<bytes, to_bytes, taraxa_evm_state_api_get_code_by_address>(this_c, blk_num, addr);
}

ExecutionResult StateAPI::dry_run_transaction(BlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
                                              optional<ExecutionOptions> const& opts) const {
  return c_method_args_rlp<ExecutionResult, from_rlp, taraxa_evm_state_api_dry_run_transaction>(this_c, blk_num, blk, trx, opts);
}

StateDescriptor StateAPI::get_last_committed_state_descriptor() const {
  StateDescriptor ret;
  taraxa_evm_state_api_get_last_committed_state_descriptor(this_c, decoder_cb_c<StateDescriptor, from_rlp>(ret), err_handler_c);
  return ret;
}

StateTransitionResult const& StateAPI::transition_state(EVMBlock const& block,  //
                                                        RangeView<EVMTransaction> const& transactions, RangeView<UncleBlock> const& uncles) {
  result_buf_transition_state.ExecutionResults.clear();
  rlp_enc_transition_state.clear();
  c_method_args_rlp<StateTransitionResult, from_rlp, taraxa_evm_state_api_transition_state>(this_c, rlp_enc_transition_state,
                                                                                            result_buf_transition_state, block, transactions, uncles);
  return result_buf_transition_state;
}

void StateAPI::transition_state_commit() { taraxa_evm_state_api_transition_state_commit(this_c, err_handler_c); }

void StateAPI::create_snapshot(uint64_t const& period) {
  auto path = db_path + to_string(period);
  GoString go_path;
  go_path.p = path.c_str();
  go_path.n = path.size();
  taraxa_evm_state_api_db_snapshot(this_c, go_path, 0, err_handler_c);
}

uint64_t StateAPI::dpos_eligible_count(BlockNumber blk_num) const { return taraxa_evm_state_api_dpos_eligible_count(this_c, blk_num, err_handler_c); }

bool StateAPI::dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const {
  RLPStream rlp;
  rlp.reserve(sizeof(BlockNumber) + sizeof(addr_t) + 8, 1);
  enc_rlp_tuple(rlp, blk_num, addr);
  return taraxa_evm_state_api_dpos_is_eligible(this_c, map_bytes(rlp.out()), err_handler_c);
}

addr_t const& StateAPI::dpos_contract_addr() {
  static auto const ret = [] {
    auto ret_c = taraxa_evm_state_api_dpos_contract_addr();
    return addr_t(ret_c.Val, addr_t::ConstructFromPointer);
  }();
  return ret;
}

StateAPI::DPOSTransactionPrototype::DPOSTransactionPrototype(DPOSTransfers const& transfers) {
  RLPStream transfers_rlp;
  enc_rlp(transfers_rlp, transfers);
  input = transfers_rlp.invalidate();
}

void enc_rlp(RLPStream& rlp, ExecutionOptions const& obj) { enc_rlp_tuple(rlp, obj.disable_nonce_check, obj.disable_gas_fee); }

void enc_rlp(RLPStream& rlp, ETHChainConfig const& obj) {
  enc_rlp_tuple(rlp, obj.homestead_block, obj.dao_fork_block, obj.eip_150_block, obj.eip_158_block, obj.byzantium_block, obj.constantinople_block,
                obj.petersburg_block);
}

u256 ChainConfig::effective_genesis_balance(addr_t const& addr) const {
  if (!genesis_balances.count(addr)) {
    return 0;
  }
  auto ret = genesis_balances.at(addr);
  if (dpos && dpos->genesis_state.count(addr)) {
    for (auto const& [_, val] : dpos->genesis_state.at(addr)) {
      ret -= val;
    }
  }
  return ret;
}

void enc_rlp(RLPStream& rlp, ChainConfig const& obj) {
  enc_rlp_tuple(rlp, obj.eth_chain_config, obj.disable_block_rewards, obj.execution_options, obj.genesis_balances, obj.dpos);
}

void enc_rlp(RLPStream& rlp, EVMBlock const& obj) { enc_rlp_tuple(rlp, obj.Author, obj.GasLimit, obj.Time, obj.Difficulty); }

void enc_rlp(RLPStream& rlp, EVMTransaction const& obj) {
  enc_rlp_tuple(rlp, obj.From, obj.GasPrice, obj.To ? obj.To->ref() : bytesConstRef(), obj.Nonce, obj.Value, obj.Gas, obj.Input);
}

void enc_rlp(RLPStream& rlp, EVMTransactionWithHash const& obj) { enc_rlp_tuple(rlp, obj.Hash, obj.Transaction); }

void enc_rlp(RLPStream& rlp, UncleBlock const& obj) { enc_rlp_tuple(rlp, obj.Number, obj.Author); }

void enc_rlp(RLPStream& rlp, Opts const& obj) { enc_rlp_tuple(rlp, obj.ExpectedMaxTrxPerBlock, obj.MainTrieFullNodeLevelsToCache); }

void dec_rlp(RLP const& rlp, Account& obj) { dec_rlp_tuple(rlp, obj.Nonce, obj.Balance, obj.StorageRootHash, obj.CodeHash, obj.CodeSize); }

void dec_rlp(RLP const& rlp, LogRecord& obj) { dec_rlp_tuple(rlp, obj.Address, obj.Topics, obj.Data); }

void dec_rlp(RLP const& rlp, ExecutionResult& obj) {
  dec_rlp_tuple(rlp, obj.CodeRet, obj.NewContractAddr, obj.Logs, obj.GasUsed, obj.CodeErr, obj.ConsensusErr);
}

void dec_rlp(RLP const& rlp, StateTransitionResult& obj) { dec_rlp_tuple(rlp, obj.ExecutionResults, obj.StateRoot); }

void dec_rlp(RLP const& rlp, TrieProof& obj) { dec_rlp_tuple(rlp, obj.Value, obj.Nodes); }

void dec_rlp(RLP const& rlp, Proof& obj) { dec_rlp_tuple(rlp, obj.AccountProof, obj.StorageProofs); }

void dec_rlp(RLP const& rlp, UncleBlock& obj) { dec_rlp_tuple(rlp, obj.Number, obj.Author); }

void dec_rlp(RLP const& rlp, EVMBlock& obj) { dec_rlp_tuple(rlp, obj.Author, obj.GasLimit, obj.Time, obj.Difficulty); }

void dec_rlp(RLP const& rlp, EVMTransaction& obj) { dec_rlp_tuple(rlp, obj.From, obj.GasPrice, obj.To, obj.Nonce, obj.Value, obj.Gas, obj.Input); }

Json::Value enc_json(ExecutionOptions const& obj) {
  Json::Value json(Json::objectValue);
  json["disable_nonce_check"] = obj.disable_nonce_check;
  json["disable_gas_fee"] = obj.disable_gas_fee;
  return json;
}

void dec_json(Json::Value const& json, ExecutionOptions& obj) {
  obj.disable_nonce_check = json["disable_nonce_check"].asBool();
  obj.disable_gas_fee = json["disable_gas_fee"].asBool();
}

Json::Value enc_json(ETHChainConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["homestead_block"] = dev::toJS(obj.homestead_block);
  json["dao_fork_block"] = dev::toJS(obj.dao_fork_block);
  json["eip_150_block"] = dev::toJS(obj.eip_150_block);
  json["eip_158_block"] = dev::toJS(obj.eip_158_block);
  json["byzantium_block"] = dev::toJS(obj.byzantium_block);
  json["constantinople_block"] = dev::toJS(obj.constantinople_block);
  json["petersburg_block"] = dev::toJS(obj.petersburg_block);
  return json;
}

void dec_json(Json::Value const& json, ETHChainConfig& obj) {
  obj.homestead_block = dev::jsToInt(json["homestead_block"].asString());
  obj.dao_fork_block = dev::jsToInt(json["dao_fork_block"].asString());
  obj.eip_150_block = dev::jsToInt(json["eip_150_block"].asString());
  obj.eip_158_block = dev::jsToInt(json["eip_158_block"].asString());
  obj.byzantium_block = dev::jsToInt(json["byzantium_block"].asString());
  obj.constantinople_block = dev::jsToInt(json["constantinople_block"].asString());
  obj.petersburg_block = dev::jsToInt(json["petersburg_block"].asString());
}

Json::Value enc_json(ChainConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["eth_chain_config"] = enc_json(obj.eth_chain_config);
  json["execution_options"] = enc_json(obj.execution_options);
  json["disable_block_rewards"] = obj.disable_block_rewards;
  json["genesis_balances"] = enc_json(obj.genesis_balances);
  if (obj.dpos) {
    json["dpos"] = enc_json(*obj.dpos);
  }
  return json;
}

void dec_json(Json::Value const& json, ChainConfig& obj) {
  dec_json(json["eth_chain_config"], obj.eth_chain_config);
  dec_json(json["execution_options"], obj.execution_options);
  obj.disable_block_rewards = json["disable_block_rewards"].asBool();
  dec_json(json["genesis_balances"], obj.genesis_balances);
  if (auto const& dpos = json["dpos"]; !dpos.isNull()) {
    dec_json(dpos, obj.dpos.emplace());
  }
}

Json::Value enc_json(BalanceMap const& obj) {
  Json::Value json(Json::objectValue);
  for (auto const& [k, v] : obj) {
    json[dev::toJS(k)] = dev::toJS(v);
  }
  return json;
}

void dec_json(Json::Value const& json, BalanceMap& obj) {
  for (auto const& k : json.getMemberNames()) {
    obj[addr_t(k)] = dev::jsToU256(json[k].asString());
  }
}

void enc_rlp(RLPStream& enc, DPOSConfig const& obj) {
  enc_rlp_tuple(enc, obj.eligibility_balance_threshold, obj.deposit_delay, obj.withdrawal_delay, obj.genesis_state);
}

Json::Value enc_json(DPOSConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["eligibility_balance_threshold"] = dev::toJS(obj.eligibility_balance_threshold);
  json["deposit_delay"] = dev::toJS(obj.deposit_delay);
  json["withdrawal_delay"] = dev::toJS(obj.withdrawal_delay);
  auto& genesis_state = json["genesis_state"] = Json::Value(Json::objectValue);
  for (auto const& [k, v] : obj.genesis_state) {
    genesis_state[dev::toJS(k)] = enc_json(v);
  }
  return json;
}

void dec_json(Json::Value const& json, DPOSConfig& obj) {
  obj.eligibility_balance_threshold = dev::jsToU256(json["eligibility_balance_threshold"].asString());
  obj.deposit_delay = dev::jsToInt(json["deposit_delay"].asString());
  obj.withdrawal_delay = dev::jsToInt(json["withdrawal_delay"].asString());
  auto const& genesis_state = json["genesis_state"];
  for (auto const& k : genesis_state.getMemberNames()) {
    dec_json(genesis_state[k], obj.genesis_state[addr_t(k)]);
  }
}

void dec_rlp(RLP const& rlp, StateDescriptor& obj) { dec_rlp_tuple(rlp, obj.blk_num, obj.state_root); }

void enc_rlp(RLPStream& enc, DPOSTransfer const& obj) { enc_rlp_tuple(enc, obj.value, obj.negative); }

}  // namespace taraxa::state_api