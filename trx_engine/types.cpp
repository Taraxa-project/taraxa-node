#include "types.hpp"

#include <json/value.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>

namespace taraxa::trx_engine::types {
using dev::h256s;
using dev::toJS;
using dev::jsToBytes;
using dev::jsToU256;
using dev::jsToInt;
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
//  receipts.reserve(receipts_json.size())
  for (auto const& receipt_json : receipts_json) {
    auto const& logs_json = receipt_json["logs"];
    LogEntries logs(logs_json.size());
    for (auto const& log_json : logs_json) {
      auto const& topics_json = log_json["topics"];
      h256s topics(topics_json.size());
      for (auto const& topic_json : topics_json) {
        topics.emplace_back(topic_json.asString());
      }
      logs.emplace_back(Address(receipt_json["address"].asString()),
                        topics,  //
                        jsToBytes(receipt_json["data"].asString()));
    }
    auto cumulative_gas_used = jsToU256(json["cumulativeGasUsed"].asString());
    if (auto const& root_json = json["root"]; !root_json.isNull()) {
      h256 root(root_json.asString());
      receipts.emplace_back(root, cumulative_gas_used, logs);
    } else {
      auto status = jsToInt(json["status"].asString());
      receipts.emplace_back(status, cumulative_gas_used, logs);
    }
  }
  auto const& trx_outputs_json = json["transactionOutputs"];
  vector<TransactionOutput> trx_outputs(trx_outputs_json.size());
  for (auto const& obj : trx_outputs_json) {
    trx_outputs.push_back(TransactionOutput::fromJson(obj));
  }
  auto const& upd_bal_json = json["updatedBalances"];
  unordered_map<Address, u256> updated_balances(upd_bal_json.size());
  for (auto i = upd_bal_json.begin(), end = upd_bal_json.end(); i != end; ++i) {
    auto const& key = i.key().asString();
    auto const& value = (*i).asString();
    updated_balances[dev::Address(key)] = jsToU256(value);
  }
  return {
      h256(json["stateRoot"].asString()),
      jsToU256(json["usedGas"].asString()),
      move(receipts),
      move(trx_outputs),
      move(updated_balances),
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