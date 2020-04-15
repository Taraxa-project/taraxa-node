#include "types.hpp"

#include <json/value.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>

namespace taraxa::trx_engine::types {
using dev::h256s;
using dev::jsToBytes;
using dev::jsToInt;
using dev::jsToU256;
using dev::toJS;
using dev::eth::LogEntries;
using dev::eth::TransactionReceipt;
using std::move;

TransactionOutput TransactionOutput::fromJson(Json::Value const& json) {
  auto const& error_json = json["error"];
  return {
      jsToBytes(json["returnValue"].asString()),
      error_json.isNull() ? "" : error_json.asString(),
  };
}

StateTransitionResult StateTransitionResult::fromJson(Json::Value const& json) {
  auto const& receipts_json = json["receipts"];
  TransactionReceipts receipts;
  receipts.reserve(receipts_json.size());
  for (auto const& receipt_json : receipts_json) {
    auto const& logs_json = receipt_json["logs"];
    LogEntries logs;
    logs.reserve(logs_json.size());
    for (auto const& log_json : logs_json) {
      auto const& topics_json = log_json["topics"];
      h256s topics;
      topics.reserve(topics_json.size());
      std::transform(topics_json.begin(), topics_json.end(),
                     std::back_inserter(topics),
                     [](Json::Value const& topic_json) {
                       return dev::h256(topic_json.asString());
                     });
      logs.emplace_back(Address(log_json["address"].asString()),
                        topics,  //
                        jsToBytes(log_json["data"].asString()));
    }
    auto cumulative_gas_used =
        jsToU256(receipt_json["cumulativeGasUsed"].asString());
    if (auto const& root_json = receipt_json["root"]; !root_json.isNull()) {
      receipts.emplace_back(h256(root_json.asString()), cumulative_gas_used,
                            logs);
    } else {
      auto status = jsToInt(receipt_json["status"].asString());
      receipts.emplace_back(status, cumulative_gas_used, logs);
    }
  }
  auto const& trx_outputs_json = json["transactionOutputs"];
  vector<TransactionOutput> trx_outputs;
  trx_outputs.reserve(trx_outputs_json.size());
  std::transform(trx_outputs_json.begin(), trx_outputs_json.end(),
                 std::back_inserter(trx_outputs), [](Json::Value const& obj) {
                   return TransactionOutput::fromJson(obj);
                 });
  auto const& upd_bal_json = json["touchedExternallyOwnedAccountBalances"];
  unordered_map<Address, u256> touched_externally_owned_account_balances;
  touched_externally_owned_account_balances.reserve(upd_bal_json.size());
  for (auto i = upd_bal_json.begin(), end = upd_bal_json.end(); i != end; ++i) {
    auto const& key = i.key().asString();
    auto const& value = (*i).asString();
    touched_externally_owned_account_balances[dev::Address(key)] =
        jsToU256(value);
  }
  return {
      h256(json["stateRoot"].asString()),
      jsToU256(json["usedGas"].asString()),
      move(receipts),
      move(trx_outputs),
      move(touched_externally_owned_account_balances),
  };
}

Json::Value Block::toJson() const {
  Json::Value ret;
  ret["miner"] = toJS(miner);
  // TODO switch to hex str
  ret["number"] = Json::UInt64(number);
  ret["difficulty"] = toJS(difficulty);
  ret["time"] = toJS(time);
  ret["gasLimit"] = toJS(gasLimit);
  Json::Value transactions_json(Json::arrayValue);
  for (auto& trx : transactions) {
    auto trx_json = dev::eth::toJson(trx);
    trx_json["input"] = trx_json["data"];
    transactions_json.append(move(trx_json));
  }
  ret["transactions"] = transactions_json;
  return ret;
}

Json::Value StateTransitionRequest::toJson() const {
  Json::Value ret;
  ret["stateRoot"] = toJS(stateRoot);
  ret["block"] = block.toJson();
  return ret;
}

}  // namespace taraxa::trx_engine::types