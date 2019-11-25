#include "JsonHelper.h"
#include <jsonrpccpp/common/exception.h>
#include <libethcore/CommonJS.h>
#include <libwebthree/WebThree.h>

using namespace std;
using namespace dev;
using namespace eth;
using namespace p2p;

namespace taraxa::net {

Json::Value toJson(unordered_map<u256, u256> const& _storage) {
  Json::Value res(Json::objectValue);
  for (auto i : _storage) res[toJS(i.first)] = toJS(i.second);
  return res;
}

Json::Value toJson(map<h256, pair<u256, u256>> const& _storage) {
  Json::Value res(Json::objectValue);
  for (auto i : _storage)
    res[toJS(u256(i.second.first))] = toJS(i.second.second);
  return res;
}

Json::Value toJson(Address const& _address) { return toJS(_address); }

Json::Value toJson(p2p::PeerSessionInfo const& _p) {
  //@todo localAddress
  //@todo protocols
  Json::Value ret;
  ret["id"] = _p.id.hex();
  ret["name"] = _p.clientVersion;
  ret["network"]["remoteAddress"] = _p.host + ":" + toString(_p.port);
  ret["lastPing"] =
      (int)chrono::duration_cast<chrono::milliseconds>(_p.lastPing).count();
  for (auto const& i : _p.notes) ret["notes"][i.first] = i.second;
  for (auto const& i : _p.caps)
    ret["caps"].append(i.first + "/" + toString((unsigned)i.second));
  return ret;
}

Json::Value toJson(taraxa::Transaction const& _t,
                   optional<taraxa::TransactionPosition> const& pos) {
  Json::Value tr_js;
  if (pos) {
    tr_js["blockNumber"] = toJS(pos->block_number);
    tr_js["blockHash"] = toJS(pos->block_hash);
    tr_js["transactionIndex"] = toJS(pos->transaction_index);
  }
  tr_js["hash"] = toJS(_t.getHash());
  tr_js["input"] = toHexPrefixed(_t.getData());
  tr_js["to"] = toJS(_t.getReceiver());
  tr_js["from"] = toJS(_t.getSender());
  tr_js["gas"] = toJS(_t.getGas());
  tr_js["gasPrice"] = toJS(_t.getGasPrice());
  tr_js["nonce"] = toJS(_t.getNonce());
  tr_js["value"] = toJS(_t.getValue());
  dev::SignatureStruct sig(_t.getSig());
  tr_js["v"] = toJS(sig.v);
  tr_js["r"] = toJS(sig.r);
  tr_js["s"] = toJS(sig.s);
  return tr_js;
}

Json::Value toJson(std::shared_ptr<::taraxa::DagBlock> block,
                   uint64_t block_height) {
  Json::Value res;
  static const auto MOCK_BLOCK_GAS_LIMIT =
      toJS(std::numeric_limits<uint64_t>::max());
  res["hash"] = toJS(block->getHash());
  res["parentHash"] = toJS(block->getPivot());
  res["stateRoot"] = "";
  res["transactionsRoot"] = "";
  res["receiptsRoot"] = "";
  res["number"] = toJS(block_height);
  res["gasUsed"] = "0x0";
  res["gasLimit"] = MOCK_BLOCK_GAS_LIMIT;
  res["extraData"] = "";
  res["logsBloom"] = "";
  res["timestamp"] = toJS(block->getTimestamp());
  res["author"] = toJS(block->getSender());
  res["miner"] = toJS(block->getSender());
  res["nonce"] = "0x7bb9369dcbaec019";
  res["sha3Uncles"] = "0x0";
  res["difficulty"] = "0x0";
  res["totalDifficulty"] = "0x0";
  res["size"] = toJS(sizeof(*block));
  res["uncles"] = Json::Value(Json::arrayValue);
  res["transactions"] = Json::Value(Json::arrayValue);

  return res;
}

h256 h256fromHex(string const& _s) {
  try {
    return h256(_s);
  } catch (boost::exception const&) {
    throw jsonrpc::JsonRpcException("Invalid hex-encoded string: " + _s);
  }
}

}  // namespace taraxa::net
