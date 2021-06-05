#include "state_api_data.hpp"

#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"

namespace taraxa::state_api {

TaraxaEVMError::TaraxaEVMError(string type, string msg) : runtime_error(move(msg)), type(move(type)) {}
TaraxaEVMError::~TaraxaEVMError() throw() {}

ErrFutureBlock::~ErrFutureBlock() throw() {}

h256 const& Account::storage_root_eth() const { return StorageRootHash ? StorageRootHash : EmptyRLPListSHA3; }
h256 const& Account::code_hash_eth() const { return CodeSize ? CodeHash : EmptySHA3; }

u256 Config::effective_genesis_balance(addr_t const& addr) const {
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

Json::Value enc_json(Config const& obj) {
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

void dec_json(Json::Value const& json, Config& obj) {
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

void dec_json(Json::Value const& json, DPOSQuery::AccountQuery& obj) {
  static auto const dec_field = [](Json::Value const& json, char const* name, bool& field) {
    if (auto const& e = json[name]; !e.isNull()) {
      field = e.asBool();
    }
  };
  dec_field(json, "with_staking_balance", obj.with_staking_balance);
  dec_field(json, "with_outbound_deposits", obj.with_outbound_deposits);
  dec_field(json, "outbound_deposits_addrs_only", obj.outbound_deposits_addrs_only);
  dec_field(json, "with_inbound_deposits", obj.with_inbound_deposits);
  dec_field(json, "inbound_deposits_addrs_only", obj.inbound_deposits_addrs_only);
}

void dec_json(Json::Value const& json, DPOSQuery& obj) {
  if (auto const& e = json["with_eligible_count"]; !e.isNull()) {
    obj.with_eligible_count = e.asBool();
  }
  auto const& account_queries = json["account_queries"];
  for (auto const& k : account_queries.getMemberNames()) {
    dec_json(account_queries[k], obj.account_queries[addr_t(k)]);
  }
}

Json::Value enc_json(DPOSQueryResult::AccountResult const& obj, DPOSQuery::AccountQuery* q) {
  Json::Value json(Json::objectValue);
  if (!q || q->with_staking_balance) {
    json["staking_balance"] = dev::toJS(obj.staking_balance);
    json["is_eligible"] = obj.is_eligible;
  }
  static auto const enc_deposits = [](Json::Value& root, char const* key,
                                      decltype(obj.inbound_deposits) const& deposits, bool hydrated) {
    auto& deposits_json = root[key] = Json::Value(hydrated ? Json::objectValue : Json::arrayValue);
    for (auto const& [k, v] : deposits) {
      if (hydrated) {
        deposits_json[dev::toJS(k)] = dev::toJS(v);
      } else {
        deposits_json.append(dev::toJS(k));
      }
    }
  };
  enc_deposits(json, "outbound_deposits", obj.outbound_deposits, !q || !q->outbound_deposits_addrs_only);
  enc_deposits(json, "inbound_deposits", obj.inbound_deposits, !q || !q->inbound_deposits_addrs_only);
  return json;
}

Json::Value enc_json(DPOSQueryResult const& obj, DPOSQuery* q) {
  Json::Value json(Json::objectValue);
  if (!q || q->with_eligible_count) {
    json["eligible_count"] = dev::toJS(obj.eligible_count);
  }
  auto& account_results = json["account_results"] = Json::Value(Json::objectValue);
  for (auto const& [k, v] : obj.account_results) {
    account_results[dev::toJS(k)] = enc_json(v, q ? &q->account_queries[k] : nullptr);
  }
  return json;
}

RLP_FIELDS_DEFINE(ExecutionOptions, disable_nonce_check, disable_gas_fee)
RLP_FIELDS_DEFINE(ETHChainConfig, homestead_block, dao_fork_block, eip_150_block, eip_158_block, byzantium_block,
                  constantinople_block, petersburg_block)
RLP_FIELDS_DEFINE(DPOSConfig, eligibility_balance_threshold, deposit_delay, withdrawal_delay, genesis_state)
RLP_FIELDS_DEFINE(DPOSTransfer, value, negative)
RLP_FIELDS_DEFINE(Config, eth_chain_config, disable_block_rewards, execution_options, genesis_balances, dpos)
RLP_FIELDS_DEFINE(EVMBlock, Author, GasLimit, Time, Difficulty)
RLP_FIELDS_DEFINE(EVMTransaction, From, GasPrice, To, Nonce, Value, Gas, Input)
RLP_FIELDS_DEFINE(UncleBlock, Number, Author)
RLP_FIELDS_DEFINE(LogRecord, Address, Topics, Data)
RLP_FIELDS_DEFINE(ExecutionResult, CodeRet, NewContractAddr, Logs, GasUsed, CodeErr, ConsensusErr)
RLP_FIELDS_DEFINE(StateTransitionResult, ExecutionResults, StateRoot)
RLP_FIELDS_DEFINE(Account, Nonce, Balance, StorageRootHash, CodeHash, CodeSize)
RLP_FIELDS_DEFINE(TrieProof, Value, Nodes)
RLP_FIELDS_DEFINE(Proof, AccountProof, StorageProofs)
RLP_FIELDS_DEFINE(Opts, ExpectedMaxTrxPerBlock, MainTrieFullNodeLevelsToCache)
RLP_FIELDS_DEFINE(OptsDB, db_path, disable_most_recent_trie_value_views)
RLP_FIELDS_DEFINE(StateDescriptor, blk_num, state_root)
RLP_FIELDS_DEFINE(DPOSQuery::AccountQuery, with_staking_balance, with_outbound_deposits, outbound_deposits_addrs_only,
                  with_inbound_deposits, inbound_deposits_addrs_only)
RLP_FIELDS_DEFINE(DPOSQuery, with_eligible_count, account_queries)
RLP_FIELDS_DEFINE(DPOSQueryResult::AccountResult, staking_balance, is_eligible, outbound_deposits, inbound_deposits)
RLP_FIELDS_DEFINE(DPOSQueryResult, eligible_count, account_results)

}  // namespace taraxa::state_api