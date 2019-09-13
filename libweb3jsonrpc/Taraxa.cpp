#include "Taraxa.h"
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libethcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libwebthree/WebThree.h>
#include <csignal>
#include "AccountHolder.h"
#include "JsonHelper.h"
#include "libethereum/TransactionReceipt.h"
#include "rlp_array.hpp"

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

string Taraxa::taraxa_hashrate() { return "0x0"; }

bool Taraxa::taraxa_mining() { return false; }

string Taraxa::taraxa_gasPrice() { return "0x0"; }

Json::Value Taraxa::taraxa_accounts() {
  Json::Value ret(Json::arrayValue);
  ret.append(taraxa_coinbase());
  return ret;
}

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
  return blk ? "0x0" : JSON_NULL;
}

Json::Value Taraxa::taraxa_getUncleCountByBlockNumber(
    string const& _blockNumber) {
  auto snapshot = getSnapshot(tryGetNode(), BlockNumber::from(_blockNumber));
  return snapshot ? "0x0" : JSON_NULL;
}

string Taraxa::taraxa_getCode(string const& _address,
                              string const& _blockNumber) {
  auto state = getState(tryGetNode(), BlockNumber::from(_blockNumber));
  return state ? toJS(state->code(addr_t(_address))) : "";
}

string Taraxa::taraxa_sendTransaction(Json::Value const& _json) {
  auto node = tryGetNode();
  try {
    taraxa::Transaction trx(trx_hash_t("0x1"),
                            taraxa::Transaction::Type::Call,  //
                            val_t(std::stoul(_json["nonce"].asString(), 0, 16)),
                            val_t(std::stoul(_json["value"].asString(), 0, 16)),
                            val_t((_json["gas_price"].asString())),
                            val_t(_json["gas"].asString()),  //
                            addr_t(_json["to"].asString()),
                            taraxa::sig_t(),  //
                            str2bytes(_json["data"].asString()));
    if (node->getAddress() == addr_t(_json["from"].asString())) {
      trx.sign(node->getSecretKey());
      node->insertTransaction(trx);
      return toJS(trx.getHash());
    }
    return toJS(trx_hash_t());
  } catch (Exception const&) {
    throw JsonRpcException("sendTransaction invalid format");
  }
}

// TODO not listed at https://github.com/ethereum/wiki/wiki/JSON-RPC
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

string Taraxa::taraxa_estimateGas(Json::Value const& _json) { return "0x0"; }

// TODO not listed at https://github.com/ethereum/wiki/wiki/JSON-RPC
bool Taraxa::taraxa_flush() { return false; }

Json::Value Taraxa::taraxa_getBlockByHash(string const& _blockHash,
                                          bool _includeTransactions) {
  auto node = tryGetNode();
  auto snapshot = node->getStateRegistry()->getSnapshot(blk_hash_t(_blockHash));
  if (!snapshot) {
    return JSON_NULL;
  }
  return getBlockJson(node, *snapshot, _includeTransactions);
}

Json::Value Taraxa::taraxa_getBlockByNumber(string const& _blockNumber,
                                            bool _includeTransactions) {
  auto node = tryGetNode();
  auto snapshot = getSnapshot(node, BlockNumber::from(_blockNumber));
  if (!snapshot) {
    return JSON_NULL;
  }
  return getBlockJson(node, *snapshot, _includeTransactions);
}

Json::Value Taraxa::taraxa_getTransactionByHash(
    string const& _transactionHash) {
  auto node = tryGetNode();
  trx_hash_t trx_hash(_transactionHash);
  auto trx = node->getTransaction(trx_hash);
  if (!trx) {
    return JSON_NULL;
  }
  auto blk_hash = node->getDagBlockFromTransaction(trx_hash);
  if (blk_hash.isZero()) {
    return toJson(*trx);
  }
  auto snapshot = node->getStateRegistry()->getSnapshot(blk_hash);
  if (!snapshot) {
    return toJson(*trx);
  }
  auto const& trxs = node->getDagBlock(blk_hash)->getTrxs();
  return toJson(*trx, {{snapshot->block_number,
                        snapshot->block_hash,  //
                        *find(trxs, trx_hash)}});
}

Json::Value Taraxa::taraxa_getTransactionByBlockHashAndIndex(
    string const& _blockHash, string const& _transactionIndex) {
  auto node = tryGetNode();
  auto trx_num = to_trx_num(_transactionIndex);
  auto snapshot = node->getStateRegistry()->getSnapshot(blk_hash_t(_blockHash));
  if (!snapshot) {
    return JSON_NULL;
  }
  auto const& trxs = node->getDagBlock(snapshot->block_hash)->getTrxs();
  if (trx_num >= trxs.size()) {
    return JSON_NULL;
  }
  auto trx = node->getTransaction(trxs[trx_num]);
  return toJson(*trx, {{snapshot->block_number,
                        snapshot->block_hash,  //
                        trx_num}});
}

Json::Value Taraxa::taraxa_getTransactionByBlockNumberAndIndex(
    string const& _blockNumber, string const& _transactionIndex) {
  // TODO dedupe code
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
  return toJson(*trx, {{snapshot->block_number,
                        snapshot->block_hash,  //
                        trx_num}});
}

Json::Value Taraxa::taraxa_getTransactionReceipt(
    string const& _transactionHash) {
  auto node = tryGetNode();
  trx_hash_t trx_hash(_transactionHash);
  auto blk_hash = node->getDagBlockFromTransaction(trx_hash);
  if (blk_hash.isZero()) {
    return JSON_NULL;
  }
  auto snapshot = node->getStateRegistry()->getSnapshot(blk_hash);
  if (!snapshot) {
    return JSON_NULL;
  }
  auto trxs = node->getDagBlock(blk_hash)->getTrxs();
  Json::Value res;
  res["transactionHash"] = toJS(trx_hash);
  res["transactionIndex"] = toJS(*find(trxs, trx_hash));
  res["blockNumber"] = toJS(snapshot->block_number);
  res["blockHash"] = toJS(blk_hash);
  res["cumulativeGasUsed"] = "0x0";
  res["gasUsed"] = "0x0";
  res["contractAddress"] = JSON_NULL;
  res["logs"] = Json::Value(Json::arrayValue);
  static auto const logs_bloom = toJS(LogBloom());
  res["logsBloom"] = logs_bloom;
  res["status"] = "0x1";
  return res;
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

Json::Value Taraxa::taraxa_syncing() { return Json::Value(false); }

// TODO not listed at https://github.com/ethereum/wiki/wiki/JSON-RPC
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

optional<StateSnapshot> Taraxa::getSnapshot(NodePtr const& node,
                                            BlockNumber const& num) {
  switch (num.kind) {
    case BlockNumber::Kind::latest:
    case BlockNumber::Kind::pending:
      return node->getStateRegistry()->getCurrentSnapshot();
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
    default: {
      // TODO cache
      auto state = node->getStateRegistry()->getState(*num.block_number);
      return state ? make_shared<StateRegistry::State>(*state) : nullptr;
    }
  }
}

Json::Value Taraxa::getBlockJson(NodePtr const& node,
                                 StateSnapshot const& snapshot,  //
                                 bool include_trx) {
  auto block = node->getDagBlock(snapshot.block_hash);
  Json::Value res;
  res["number"] = toJS(snapshot.block_number);
  res["hash"] = toJS(snapshot.block_hash);
  if (snapshot.block_number != 0) {
    auto const& parent_hash = node->getStateRegistry()
                                  ->getSnapshot(snapshot.block_number - 1)  //
                                  ->block_hash;
    res["parentHash"] = toJS(parent_hash);
  } else {
    static auto const zero_hash = toJS(blk_hash_t());
    res["parentHash"] = zero_hash;
  }
  res["stateRoot"] = toJS(snapshot.state_root);
  static auto const gas_limit = toJS(MOCK_BLOCK_GAS_LIMIT);
  res["gasLimit"] = gas_limit;
  res["gasUsed"] = "0x0";
  res["extraData"] = "0x0";
  static auto const logs_bloom = toJS(LogBloom());
  res["logsBloom"] = logs_bloom;
  res["timestamp"] = toJS(block->getTimestamp());
  res["miner"] = toJS(block->getSender());
  // TODO What's "author"? This field is not present in the spec
  // https://github.com/ethereum/wiki/wiki/JSON-RPC#eth_getblockbyhash
  res["author"] = res["miner"];
  res["nonce"] = "0x7bb9369dcbaec019";
  res["difficulty"] = "0x0";
  res["totalDifficulty"] = "0x0";
  res["uncles"] = Json::Value(Json::arrayValue);
  static auto const sha3_uncles = toJS(dev::EmptyListSHA3);
  res["sha3Uncles"] = sha3_uncles;
  res["transactions"] = Json::Value(Json::arrayValue);
  RlpArray trx_rlp_array, receitps_rlp_array;
  auto const& trx_hashes = block->getTrxs();
  for (trx_num_t i = 0; i < trx_hashes.size(); ++i) {
    auto const& trx_hash = trx_hashes[i];
    auto const& trx = node->getTransaction(trx_hash);
    trx_rlp_array.append([&](auto& rlp_stream) {
      trx->streamRLP(rlp_stream, true, false);  //
    });
    receitps_rlp_array.append([](auto& rlp_stream) {
      TransactionReceipt(1, 0, {}).streamRLP(rlp_stream);
    });
    if (!include_trx) {
      res["transactions"].append(toJS(trx_hash));
      continue;
    }
    auto const& trx_js = toJson(*trx, {{snapshot.block_number,
                                        snapshot.block_hash,  //
                                        i}});
    res["transactions"].append(trx_js);
  }
  res["transactionsRoot"] = toJS(trx_rlp_array.trieRoot());
  res["receiptsRoot"] = toJS(receitps_rlp_array.trieRoot());
  res["size"] = toJS(sizeof(*block));
  return res;
}

Json::Value Taraxa::taraxa_getDagBlockByHash(string const& _blockHash,
                                             bool _includeTransactions) {
  auto node = tryGetNode();
  auto block = node->getDagBlock(blk_hash_t(_blockHash));
  if (block) {
    auto block_json = block->getJson();
    if (_includeTransactions) {
      block_json["transactions"] = Json::Value(Json::arrayValue);
      for (auto const& t : block->getTrxs()) {
        block_json["transactions"].append(node->getTransaction(t)->getJson());
      }
    }
    return block_json;
  }
  return JSON_NULL;
}

Json::Value Taraxa::taraxa_getDagBlockByLevel(string const& _blockLevel,
                                              bool _includeTransactions) {
  auto node = tryGetNode();
  auto blocks = node->getDagBlocksAtLevel(std::stoull(_blockLevel, 0, 16), 1);
  auto res = Json::Value(Json::arrayValue);
  for (auto const& b : blocks) {
    auto block_json = b->getJson();
    if (_includeTransactions) {
      block_json["transactions"] = Json::Value(Json::arrayValue);
      for (auto const& t : b->getTrxs()) {
        block_json["transactions"].append(node->getTransaction(t)->getJson());
      }
    }
    res.append(block_json);
  }
  return res;
}