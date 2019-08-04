/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file JsonHelper.cpp
 * @authors:
 *   Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "JsonHelper.h"

#include <libethcore/SealEngine.h>
//#include <libethereum/Client.h>
#include <jsonrpccpp/common/exception.h>
#include <libethcore/CommonJS.h>
#include <libwebthree/WebThree.h>
using namespace std;
using namespace dev;
using namespace eth;

namespace dev {

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

// ////////////////////////////////////////////////////////////////////////////////
// p2p
// ////////////////////////////////////////////////////////////////////////////////
namespace p2p {

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

}  // namespace p2p

// ////////////////////////////////////////////////////////////////////////////////
// taraxa
// ////////////////////////////////////////////////////////////////////////////////

Json::Value toJson(::taraxa::Transaction const& _t) {
  Json::Value tr_js;
  tr_js["hash"] = toJS(_t.getHash());
  tr_js["input"] = toJS(_t.getData());
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

Json::Value toJson(taraxa::DagBlock const& block,
                   optional<taraxa::dag_blk_num_t> const& blk_num) {
  Json::Value res;
  // block->updateHash();
  static const auto MOCK_BLOCK_GAS_LIMIT =
      toJS(std::numeric_limits<uint64_t>::max());
  res["hash"] = toJS(block.getHash());
  res["parentHash"] = toJS(block.getPivot());
  res["stateRoot"] = "";
  res["transactionsRoot"] = "";
  res["receiptsRoot"] = "";
  res["number"] = blk_num ? toJS(*blk_num) : JSON_NULL;
  res["gasUsed"] = "0x0";
  res["gasLimit"] = MOCK_BLOCK_GAS_LIMIT;
  res["extraData"] = "";
  res["logsBloom"] = "";
  if (blk_num) {
    // TODO this has to go
    res["timestamp"] = std::to_string(0x54e34e8e + *blk_num * 100);
  }
  res["author"] = "0x4e65fda2159562a496f9f3522f89122a3088497a";
  res["miner"] = "0x4e65fda2159562a496f9f3522f89122a3088497a";
  res["nonce"] = "0x7bb9369dcbaec019";
  res["sha3Uncles"] = "0x0";
  res["difficulty"] = "0x0";
  res["totalDifficulty"] = "0x0";
  res["size"] = toJS(sizeof(block));
  res["uncles"] = Json::Value(Json::arrayValue);
  res["transactions"] = Json::Value(Json::arrayValue);

  return res;
}

// ////////////////////////////////////////////////////////////////////////////////////
// rpc
// ////////////////////////////////////////////////////////////////////////////////////

namespace rpc {
h256 h256fromHex(string const& _s) {
  try {
    return h256(_s);
  } catch (boost::exception const&) {
    throw jsonrpc::JsonRpcException("Invalid hex-encoded string: " + _s);
  }
}
}  // namespace rpc

}  // namespace dev
