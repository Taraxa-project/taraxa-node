#include "final_chain/state_api_data.hpp"

#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"

namespace taraxa::state_api {

TaraxaEVMError::TaraxaEVMError(string type, string msg) : runtime_error(move(msg)), type(move(type)) {}
TaraxaEVMError::~TaraxaEVMError() throw() {}

ErrFutureBlock::~ErrFutureBlock() throw() {}

h256 const& Account::storage_root_eth() const { return storage_root_hash ? storage_root_hash : EmptyRLPListSHA3(); }

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

RLP_FIELDS_DEFINE(DPOSTransfer, value, negative)
RLP_FIELDS_DEFINE(EVMBlock, author, gas_limit, time, difficulty)
RLP_FIELDS_DEFINE(EVMTransaction, from, gas_price, to, nonce, value, gas, input)
RLP_FIELDS_DEFINE(UncleBlock, number, author)
RLP_FIELDS_DEFINE(LogRecord, address, topics, data)
RLP_FIELDS_DEFINE(ExecutionResult, code_retval, new_contract_addr, logs, gas_used, code_err, consensus_err)
RLP_FIELDS_DEFINE(StateTransitionResult, execution_results, state_root)
RLP_FIELDS_DEFINE(Account, nonce, balance, storage_root_hash, code_hash, code_size)
RLP_FIELDS_DEFINE(TrieProof, value, nodes)
RLP_FIELDS_DEFINE(Proof, account_proof, storage_proofs)
RLP_FIELDS_DEFINE(StateDescriptor, blk_num, state_root)
RLP_FIELDS_DEFINE(DPOSQuery::AccountQuery, with_staking_balance, with_outbound_deposits, outbound_deposits_addrs_only,
                  with_inbound_deposits, inbound_deposits_addrs_only)
RLP_FIELDS_DEFINE(DPOSQuery, with_eligible_count, account_queries)
RLP_FIELDS_DEFINE(DPOSQueryResult::AccountResult, staking_balance, is_eligible, outbound_deposits, inbound_deposits)
RLP_FIELDS_DEFINE(DPOSQueryResult, eligible_count, account_results)

}  // namespace taraxa::state_api