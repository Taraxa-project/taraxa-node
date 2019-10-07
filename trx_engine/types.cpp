#include "types.hpp"
#include <json/value.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>

namespace taraxa::trx_engine {
using namespace std;
using namespace dev;

void EthTransactionReceipt::fromJson(Json::Value const& json) {
  if (auto const& element = json["root"]; !element.isNull()) {
    root.emplace(jsToBytes(element.asString()));
  }
  if (auto const& element = json["status"]; !element.isNull()) {
    status.emplace(jsToInt(element.asString()));
  }
  cumulativeGasUsed = jsToU256(json["cumulativeGasUsed"].asString());
  logsBloom = eth::LogBloom(json["logsBloom"].asString());
  for (auto const& log_json : json["logs"]) {
    LogEntry logEntry;
    logEntry.fromJson(log_json);
    logs.push_back(logEntry);
  }
  transactionHash = h256(json["transactionHash"].asString());
  contractAddress = Address(json["contractAddress"].asString());
  gasUsed = jsToU256(json["gasUsed"].asString());
}

void LogEntry::fromJson(Json::Value const& json) {
  address = Address(json["address"].asString());
  for (auto const& topic_json : json["topics"]) {
    topics.emplace_back(topic_json.asString());
  }
  data = jsToBytes(json["data"].asString());
  blockNumber = jsToU256(json["blockNumber"].asString());
  transactionHash = h256(json["transactionHash"].asString());
  transactionIndex = jsToInt(json["transactionIndex"].asString());
  blockHash = h256(json["blockHash"].asString());
  logIndex = jsToInt(json["logIndex"].asString());
}

void TaraxaTransactionReceipt::fromJson(Json::Value const& json) {
  returnValue = jsToBytes(json["returnValue"].asString());
  ethereumReceipt.fromJson(json["ethereumReceipt"]);
  if (auto const& element = json["error"]; !element.isNull()) {
    error = element.asString();
  }
}

void StateTransitionResult::fromJson(Json::Value const& json) {
  stateRoot = h256(json["stateRoot"].asString());
  for (auto const& receipt_json : json["receipts"]) {
    TaraxaTransactionReceipt receipt;
    receipt.fromJson(receipt_json);
    receipts.push_back(receipt);
  }
  for (auto const& log_json : json["allLogs"]) {
    LogEntry logEntry;
    logEntry.fromJson(log_json);
    allLogs.push_back(logEntry);
  }
  {
    auto const& obj = json["updatedBalances"];
    for (auto i = obj.begin(), end = obj.end(); i != end; ++i) {
      auto const& key = i.key().asString();
      auto const& value = (*i).asString();
      updatedBalances[dev::Address(key)] = jsToU256(value);
    }
  }
  usedGas = jsToU256(json["usedGas"].asString());
}

Json::Value Transaction::toJson() const {
  Json::Value ret;
  if (to) {
    ret["to"] = toJS(to.value());
  }
  ret["from"] = toJS(from);
  ret["nonce"] = toJS(nonce);
  ret["value"] = toJS(value);
  ret["gas"] = toJS(gas);
  ret["gasPrice"] = toJS(gasPrice);
  ret["input"] = toJS(input);
  ret["hash"] = toJS(hash);
  return ret;
}

Json::Value Block::toJson() const {
  Json::Value ret;
  ret["miner"] = toJS(miner);
  // TODO switch to hex str
  ret["number"] = Json::UInt64(number);
  ret["difficulty"] = toJS(difficulty);
  ret["time"] = toJS(time);
  ret["gasLimit"] = toJS(gasLimit);
  ret["hash"] = toJS(hash);
  Json::Value transactions_json(Json::arrayValue);
  for (auto& tx : transactions) {
    transactions_json.append(tx.toJson());
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

}