#include "Taraxa.h"
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libethcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libwebthree/WebThree.h>
#include <csignal>
#include "AccountHolder.h"
#include "JsonHelper.h"

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace shh;
using namespace dev::rpc;
using namespace taraxa;

Taraxa::Taraxa(std::shared_ptr<FullNode>& _full_node)
    : full_node_(_full_node) {}

string Taraxa::taraxa_protocolVersion() {
  return toJS(dev::p2p::c_protocolVersion);
}

string Taraxa::taraxa_coinbase() { return toJS(tryGetNode()->getAddress()); }

string Taraxa::taraxa_hashrate() { return toJS("0"); }

bool Taraxa::taraxa_mining() { return false; }

string Taraxa::taraxa_gasPrice() { return toJS("0"); }

Json::Value Taraxa::taraxa_accounts() { return JSON_NULL; }

string Taraxa::taraxa_blockNumber() {
  return toJS(getSnapshot(tryGetNode(), BlockNumber::latest)->block_number);
}

string Taraxa::taraxa_getBalance(string const& _address,
                                 string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->balance(addr_t(_address))) : "";
}

string Taraxa::taraxa_getStorageAt(string const& _address,
                                   string const& _position,
                                   string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->storage(addr_t(_address), u256(_position))) : "";
}

string Taraxa::taraxa_getStorageRoot(string const& _address,
                                     string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->storageRoot(addr_t(_address))) : "";
}

Json::Value Taraxa::taraxa_pendingTransactions() {
  Json::Value ret(Json::arrayValue);
  for (auto const& [_, trx] : tryGetNode()->getNewVerifiedTrxSnapShot(false)) {
    ret.append(toJson(trx));
  }
  return ret;
}

string Taraxa::taraxa_getTransactionCount(string const& _address,
                                          string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->getNonce(addr_t(_address))) : "";
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByHash(
    string const& _blockHash) {
  auto blk = tryGetNode()->getDagBlock(blk_hash_t(_blockHash));
  return blk ? toJS(blk->getTrxs().size()) : JSON_NULL;
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByNumber(
    string const& _blockNumber) {
  auto node = tryGetNode();
  auto snapshot = getSnapshot(node, BlockNumber::from(_blockNumber));
  if (!snapshot) {
    return JSON_NULL;
  }
  return toJS(node->getDagBlock(snapshot->block_hash)->getTrxs().size());
}

Json::Value Taraxa::taraxa_getUncleCountByBlockHash(string const& _blockHash) {
  auto blk = tryGetNode()->getDagBlock(blk_hash_t(_blockHash));
  return blk ? toJS(0) : JSON_NULL;
}

Json::Value Taraxa::taraxa_getUncleCountByBlockNumber(
    string const& _blockNumber) {
  auto snapshot = getSnapshot(tryGetNode(), BlockNumber::from(_blockNumber));
  return snapshot ? toJS(0) : JSON_NULL;
}

string Taraxa::taraxa_getCode(string const& _address,
                              string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->code(addr_t(_address))) : "";
}

string Taraxa::taraxa_sendTransaction(Json::Value const& _json) {
  auto node = tryGetNode();
  taraxa::Transaction trx(trx_hash_t("0x1"),
                          taraxa::Transaction::Type::Call,  //
                          val_t(1),
                          val_t(std::stoul(_json["value"].asString(), 0, 16)),
                          val_t((_json["gas_price"].asString())),
                          val_t(_json["gas"].asString()),  //
                          addr_t(_json["to"].asString()),
                          taraxa::sig_t(),  //
                          str2bytes(_json["data"].asString()));
  node->insertTransaction(trx);
  return toJS(trx.getHash());
}

Json::Value Taraxa::taraxa_signTransaction(Json::Value const& _json) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_inspectTransaction(std::string const& _rlp) {
  return JSON_NULL;
}

string Taraxa::taraxa_sendRawTransaction(std::string const& _rlp) {
  auto full_node = tryGetNode();
  taraxa::Transaction trx(jsToBytes(_rlp, OnFailed::Throw));
  trx.updateHash();
  full_node->insertTransaction(trx);
  return toJS(trx.getHash());
}

string Taraxa::taraxa_call(Json::Value const& _json,
                           string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_estimateGas(Json::Value const& _json) {
  // Dummy data
  return toJS("0x5208");
}

bool Taraxa::taraxa_flush() { return false; }

Json::Value Taraxa::taraxa_getBlockByHash(string const& _blockHash,
                                          bool _includeTransactions) {
  auto node = tryGetNode();
  blk_hash_t hash(_blockHash);
  auto blk_num_opt = node->getStateRegistry()->getNumber(hash);
  return blockToJson(node, hash, blk_num_opt, _includeTransactions);
}

Json::Value Taraxa::taraxa_getBlockByNumber(string const& _blockNumber,
                                            bool _includeTransactions) {
  auto node = tryGetNode();
  auto snapshot = getSnapshot(node, BlockNumber::from(_blockNumber));
  if (!snapshot) {
    return JSON_NULL;
  }
  return blockToJson(node,
                     snapshot->block_hash,    //
                     snapshot->block_number,  //
                     _includeTransactions);
}

Json::Value Taraxa::taraxa_getTransactionByHash(
    string const& _transactionHash) {
  auto node = tryGetNode();
  trx_hash_t trx_hash(_transactionHash);
  auto trx = node->getTransaction(trx_hash);
  if (!trx) {
    return JSON_NULL;
  }
  auto trx_js = toJson(*trx);
  auto blk_hash = node->getDagBlockFromTransaction(trx_hash);
  if (blk_hash.isZero()) return trx_js;
  auto const& trxs = node->getDagBlock(blk_hash)->getTrxs();
  auto blk_num = node->getStateRegistry()->getNumber(blk_hash);
  if (blk_num) {
    trx_js["blockNumber"] = toJS(*blk_num);
  }
  trx_js["blockHash"] = toJS(blk_hash);
  trx_js["transactionIndex"] = toJS(*taraxa::find(trxs, trx_hash));
  return trx_js;
}

Json::Value Taraxa::taraxa_getTransactionByBlockHashAndIndex(
    string const& _blockHash, string const& _transactionIndex) {
  auto node = tryGetNode();
  auto trx_num = to_trx_num(_transactionIndex);
  blk_hash_t blk_hash(_blockHash);
  auto blk = node->getDagBlock(blk_hash);
  if (!blk) {
    return JSON_NULL;
  }
  auto const& trxs = blk->getTrxs();
  if (trx_num >= trxs.size()) {
    return JSON_NULL;
  }
  auto trx = node->getTransaction(trxs[trx_num]);
  auto trx_js = toJson(*trx);
  auto blk_num = node->getStateRegistry()->getNumber(blk_hash);
  if (blk_num) {
    trx_js["blockNumber"] = toJS(*blk_num);
  }
  trx_js["blockHash"] = toJS(blk_hash);
  trx_js["transactionIndex"] = toJS(trx_num);
  return trx_js;
}

Json::Value Taraxa::taraxa_getTransactionByBlockNumberAndIndex(
    string const& _blockNumber, string const& _transactionIndex) {
  auto node = tryGetNode();
  auto trx_num = to_trx_num(_transactionIndex);
  auto snapshot = getSnapshot(node, BlockNumber::from(_blockNumber));
  if (!snapshot) {
    return JSON_NULL;
  }
  auto const& trxs = node->getDagBlock(snapshot->block_hash)->getTrxs();
  if (trx_num >= trxs.size()) {
    return JSON_NULL;
  }
  auto trx = node->getTransaction(trxs[trx_num]);
  auto trx_js = toJson(*trx);
  trx_js["blockNumber"] = toJS(snapshot->block_number);
  trx_js["blockHash"] = toJS(snapshot->block_hash);
  trx_js["transactionIndex"] = toJS(trx_num);
  return trx_js;
}

Json::Value Taraxa::taraxa_getTransactionReceipt(
    string const& _transactionHash) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getUncleByBlockHashAndIndex(
    string const& _blockHash, string const& _uncleIndex) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getUncleByBlockNumberAndIndex(
    string const& _blockNumber, string const& _uncleIndex) {
  return JSON_NULL;
}

string Taraxa::taraxa_newFilter(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newFilterEx(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newBlockFilter() { return ""; }

string Taraxa::taraxa_newPendingTransactionFilter() { return ""; }

bool Taraxa::taraxa_uninstallFilter(string const& _filterId) { return false; }

Json::Value Taraxa::taraxa_getFilterChanges(string const& _filterId) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getFilterChangesEx(string const& _filterId) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getFilterLogs(string const& _filterId) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getFilterLogsEx(string const& _filterId) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getLogs(Json::Value const& _json) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getLogsEx(Json::Value const& _json) {
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getWork() { return JSON_NULL; }

Json::Value Taraxa::taraxa_syncing() { return JSON_NULL; }

string Taraxa::taraxa_chainId() { return ""; }

bool Taraxa::taraxa_submitWork(string const& _nonce, string const&,
                               string const& _mixHash) {
  return false;
}

bool Taraxa::taraxa_submitHashrate(string const& _hashes, string const& _id) {
  return false;
}

string Taraxa::taraxa_register(string const& _address) { return ""; }

bool Taraxa::taraxa_unregister(string const& _accountId) { return false; }

Json::Value Taraxa::taraxa_fetchQueuedTransactions(string const& _accountId) {
  return JSON_NULL;
}

Taraxa::NodePtr Taraxa::tryGetNode() {
  if (auto full_node = full_node_.lock()) {
    return full_node;
  }
  BOOST_THROW_EXCEPTION(
      jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR));
}

optional<StateRegistry::Snapshot> Taraxa::getSnapshot(NodePtr const& node,
                                                      BlockNumber const& num) {
  switch (num.kind) {
    case BlockNumber::Kind::earliest:
    case BlockNumber::Kind::specific: {
      auto const& state = getState(node, num);
      return state ? optional(state->getSnapshot()) : nullopt;
    }
    default:
      return node->getStateRegistry()->getSnapshot(*num.block_number);
  }
}

shared_ptr<StateRegistry::State> Taraxa::getState(NodePtr const& node,
                                                  BlockNumber const& num) {
  switch (num.kind) {
    case BlockNumber::Kind::latest:
    case BlockNumber::Kind::pending:
      return node->updateAndGetState();
    case BlockNumber::Kind::earliest:
    case BlockNumber::Kind::specific: {
      // TODO cache
      auto state = node->getStateRegistry()->getState(*num.block_number);
      return state ? make_shared<StateRegistry::State>(*state) : nullptr;
    }
  }
}

Json::Value Taraxa::blockToJson(NodePtr const& node,
                                blk_hash_t const& hash,  //
                                optional<taraxa::dag_blk_num_t> const& num,
                                bool include_trx) {
  auto blk = node->getDagBlock(hash);
  auto blk_js = toJson(*blk, num);
  taraxa::trx_num_t trx_added(0);
  for (auto const& trx_hash : blk->getTrxs()) {
    if (!include_trx) {
      blk_js["transactions"].append(toJS(trx_hash));
      continue;
    }
    auto const& trx = node->getTransaction(trx_hash);
    auto trx_js = toJson(*trx);
    trx_js["blockHash"] = blk_js["hash"];
    trx_js["blockNumber"] = blk_js["number"];
    trx_js["transactionIndex"] = toJS(trx_added);
    blk_js["transactions"].append(trx_js);
    ++trx_added;
  }
  return blk_js;
}