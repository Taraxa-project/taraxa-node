#include "JsonHelper.h"

#include <jsonrpccpp/common/exception.h>

#include "Transaction.h"

using namespace std;
using namespace dev;

namespace taraxa::aleth {

Json::Value toJson(unordered_map<u256, u256> const& _storage) {
  Json::Value res(Json::objectValue);
  for (auto i : _storage) res[toJS(i.first)] = toJS(i.second);
  return res;
}

Json::Value toJson(map<h256, pair<u256, u256>> const& _storage) {
  Json::Value res(Json::objectValue);
  for (auto i : _storage) res[toJS(u256(i.second.first))] = toJS(i.second.second);
  return res;
}

Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi) {
  Json::Value res;
  DEV_IGNORE_EXCEPTIONS(res["hash"] = toJS(_bi.hash()));
  res["parentHash"] = toJS(_bi.parentHash());
  res["sha3Uncles"] = toJS(_bi.sha3Uncles());
  res["author"] = toJS(_bi.author());
  res["stateRoot"] = toJS(_bi.stateRoot());
  res["transactionsRoot"] = toJS(_bi.transactionsRoot());
  res["receiptsRoot"] = toJS(_bi.receiptsRoot());
  res["number"] = toJS(_bi.number());
  res["gasUsed"] = toJS(_bi.gasUsed());
  res["gasLimit"] = toJS(_bi.gasLimit());
  res["extraData"] = toJS(_bi.extraData());
  res["logsBloom"] = toJS(_bi.logBloom());
  res["timestamp"] = toJS(_bi.timestamp());
  // TODO: remove once JSONRPC spec is updated to use "author" over "miner".
  res["miner"] = toJS(_bi.author());
  res["mixHash"] = toJS(_bi.mixHash());
  res["nonce"] = toJS(_bi.nonce());
  return res;
}

Json::Value toJson(::taraxa::aleth::Transaction const& _t, std::pair<h256, unsigned> _location,
                   BlockNumber _blockNumber) {
  Json::Value res;
  res["hash"] = toJS(_t.sha3());
  res["input"] = toJS(_t.data());
  res["to"] = _t.isCreation() ? Json::Value() : toJS(_t.receiveAddress());
  res["from"] = toJS(_t.safeSender());
  res["gas"] = toJS(_t.gas());
  res["gasPrice"] = toJS(_t.gasPrice());
  res["nonce"] = toJS(_t.nonce());
  res["value"] = toJS(_t.value());
  res["blockHash"] = toJS(_location.first);
  res["transactionIndex"] = toJS(_location.second);
  res["blockNumber"] = toJS(_blockNumber);
  res["v"] = toJS(_t.signature().v);
  res["r"] = toJS(_t.signature().r);
  res["s"] = toJS(_t.signature().s);
  return res;
}

Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi, BlockDetails const& _bd, UncleHashes const& _us,
                   std::vector<::taraxa::aleth::Transaction> const& _ts) {
  Json::Value res = toJson(_bi);
  res["totalDifficulty"] = toJS(_bd.totalDifficulty);
  res["size"] = toJS(_bd.size);
  res["uncles"] = Json::Value(Json::arrayValue);
  for (h256 h : _us) res["uncles"].append(toJS(h));
  res["transactions"] = Json::Value(Json::arrayValue);
  for (unsigned i = 0; i < _ts.size(); i++)
    res["transactions"].append(toJson(_ts[i], std::make_pair(_bi.hash(), i), (BlockNumber)_bi.number()));
  return res;
}

Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi, BlockDetails const& _bd, UncleHashes const& _us,
                   TransactionHashes const& _ts) {
  Json::Value res = toJson(_bi);
  res["totalDifficulty"] = toJS(_bd.totalDifficulty);
  res["size"] = toJS(_bd.size);
  res["uncles"] = Json::Value(Json::arrayValue);
  for (h256 h : _us) res["uncles"].append(toJS(h));
  res["transactions"] = Json::Value(Json::arrayValue);
  for (h256 const& t : _ts) res["transactions"].append(toJS(t));
  return res;
}

Json::Value toJson(::taraxa::aleth::TransactionReceipt const& _t) {
  Json::Value res;
  if (_t.hasStatusCode())
    res["status"] = toString(_t.statusCode());
  else
    res["stateRoot"] = toJS(_t.stateRoot());
  res["gasUsed"] = toJS(_t.cumulativeGasUsed());
  res["bloom"] = toJS(_t.bloom());
  res["log"] = toJson(_t.log());
  return res;
}

Json::Value toJson(::taraxa::aleth::LocalisedTransactionReceipt const& _t) {
  Json::Value res;
  res["transactionHash"] = toJS(_t.hash());
  res["transactionIndex"] = toJS(_t.transactionIndex());
  res["blockHash"] = toJS(_t.blockHash());
  res["blockNumber"] = toJS(_t.blockNumber());
  res["from"] = toJS(_t.from());
  res["to"] = toJS(_t.to());
  res["cumulativeGasUsed"] = toJS(_t.cumulativeGasUsed());
  res["gasUsed"] = toJS(_t.gasUsed());
  res["contractAddress"] = toJS(_t.contractAddress());
  res["logs"] = toJson(_t.localisedLogs());
  res["logsBloom"] = toJS(_t.bloom());
  if (_t.hasStatusCode())
    res["status"] = toString(_t.statusCode());
  else
    res["stateRoot"] = toJS(_t.stateRoot());
  return res;
}

Json::Value toJson(::taraxa::aleth::Transaction const& _t) {
  Json::Value res;
  res["to"] = _t.isCreation() ? Json::Value() : toJS(_t.to());
  res["from"] = toJS(_t.from());
  res["gas"] = toJS(_t.gas());
  res["gasPrice"] = toJS(_t.gasPrice());
  res["value"] = toJS(_t.value());
  res["data"] = toJS(_t.data(), 32);
  res["nonce"] = toJS(_t.nonce());
  res["hash"] = toJS(_t.sha3(WithSignature));
  res["sighash"] = toJS(_t.sha3(WithoutSignature));
  res["r"] = toJS(_t.signature().r);
  res["s"] = toJS(_t.signature().s);
  res["v"] = toJS(_t.signature().v);
  return res;
}

Json::Value toJson(::taraxa::aleth::Transaction const& _t, bytes const& _rlp) {
  Json::Value res;
  res["raw"] = toJS(_rlp);
  res["tx"] = toJson(_t);
  return res;
}

Json::Value toJson(::taraxa::aleth::LocalisedTransaction const& _t) {
  Json::Value res;
  if (_t) {
    res["hash"] = toJS(_t.sha3());
    res["input"] = toJS(_t.data());
    res["to"] = _t.isCreation() ? Json::Value() : toJS(_t.receiveAddress());
    res["from"] = toJS(_t.safeSender());
    res["gas"] = toJS(_t.gas());
    res["gasPrice"] = toJS(_t.gasPrice());
    res["nonce"] = toJS(_t.nonce());
    res["value"] = toJS(_t.value());
    res["blockHash"] = toJS(_t.blockHash());
    res["transactionIndex"] = toJS(_t.transactionIndex());
    res["blockNumber"] = toJS(_t.blockNumber());
  }
  return res;
}

Json::Value toJson(::taraxa::aleth::LocalisedLogEntry const& _e) {
  auto res = toJson(static_cast<::taraxa::aleth::LogEntry const&>(_e));
  res["blockNumber"] = toJS(_e.blockNumber);
  res["blockHash"] = toJS(_e.blockHash);
  res["logIndex"] = toJS(_e.logIndex);
  res["transactionHash"] = toJS(_e.transactionHash);
  res["transactionIndex"] = toJS(_e.transactionIndex);
  return res;
}

Json::Value toJson(::taraxa::aleth::LogEntry const& _e) {
  Json::Value res;
  res["data"] = toJS(_e.data);
  res["address"] = toJS(_e.address);
  res["topics"] = Json::Value(Json::arrayValue);
  for (auto const& t : _e.topics) res["topics"].append(toJS(t));
  return res;
}

TransactionSkeleton toTransactionSkeleton(Json::Value const& _json) {
  TransactionSkeleton ret;
  if (!_json.isObject() || _json.empty()) return ret;

  if (!_json["from"].empty()) ret.from = toAddress(_json["from"].asString());
  if (!_json["to"].empty() && _json["to"].asString() != "0x" && !_json["to"].asString().empty())
    ret.to = toAddress(_json["to"].asString());

  if (!_json["value"].empty()) ret.value = jsToU256(_json["value"].asString());

  if (!_json["gas"].empty()) ret.gas = jsToInt(_json["gas"].asString());

  if (!_json["gasPrice"].empty()) ret.gasPrice = jsToU256(_json["gasPrice"].asString());

  if (!_json["data"].empty())  // ethereum.js has preconstructed the data array
    ret.data = jsToBytes(_json["data"].asString(), OnFailed::Throw);

  if (!_json["code"].empty()) ret.data = jsToBytes(_json["code"].asString(), OnFailed::Throw);

  if (!_json["nonce"].empty()) ret.nonce = jsToInt(_json["nonce"].asString());
  return ret;
}

}  // namespace taraxa::aleth
