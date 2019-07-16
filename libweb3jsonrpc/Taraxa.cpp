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

Taraxa::Taraxa(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

string Taraxa::taraxa_protocolVersion() {
  return toJS(dev::p2p::c_protocolVersion);
}

string Taraxa::taraxa_coinbase() {
  if (auto full_node = full_node_.lock()) {
    return toJS(full_node->getAddress());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

string Taraxa::taraxa_hashrate() { return toJS("0"); }

bool Taraxa::taraxa_mining() { return false; }

string Taraxa::taraxa_gasPrice() { return toJS("0"); }

Json::Value Taraxa::taraxa_accounts() { return Json::Value(); }

string Taraxa::taraxa_blockNumber() {
  if (auto full_node = full_node_.lock()) {
    return toJS(full_node->getDagMaxLevel());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

string Taraxa::taraxa_getBalance(string const& _address,
                                 string const& _blockNumber) {
  if (auto full_node = full_node_.lock()) {
    return toJS(full_node->getBalance(taraxa::addr_t(_address)).first);
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

string Taraxa::taraxa_getStorageAt(string const& _address,
                                   string const& _position,
                                   string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_getStorageRoot(string const& _address,
                                     string const& _blockNumber) {
  return "";
}

Json::Value Taraxa::taraxa_pendingTransactions() {
  if (auto full_node = full_node_.lock()) {
    auto trxs = full_node->getNewVerifiedTrxSnapShot(false);
    auto js_trxs = Json::Value(Json::arrayValue);
    for (auto& trx : trxs) {
      Json::Value tr_js =
          dev::toJson(std::make_shared<taraxa::Transaction>(trx.second));
      js_trxs["transactions"].append(tr_js);
    }
    return js_trxs;
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

string Taraxa::taraxa_getTransactionCount(string const& _address,
                                          string const& _blockNumber) {
  return toJS("0");//TO DO look up nonce for the address
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByHash(
    string const& _blockHash) {
  if (auto full_node = full_node_.lock()) {
    auto block = full_node->getDagBlock(taraxa::blk_hash_t(_blockHash));
    if (block) return toJS(block->getTrxs().size());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByNumber(
    string const& _blockNumber) {
  if (auto full_node = full_node_.lock()) {
    auto blocks = full_node->getDagBlocksAtLevel(std::stoi(_blockNumber), 1);
    if (blocks.size() > 0) return toJS(blocks[0]->getTrxs().size());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getUncleCountByBlockHash(string const& _blockHash) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getUncleCountByBlockNumber(
    string const& _blockNumber) {
  return Json::Value();
}

string Taraxa::taraxa_getCode(string const& _address,
                              string const& _blockNumber) {
  // Dummy data
  return toJS(
      "0x600160008035811a818181146012578301005b601b6001356025565b80600052602060"
      "00f25b600060078202905091905056");
}

string Taraxa::taraxa_sendTransaction(Json::Value const& _json) {
  if (auto full_node = full_node_.lock()) {
    taraxa::Transaction trx(
        taraxa::trx_hash_t("0x1"), taraxa::Transaction::Type::Call,
        taraxa::bal_t(1), taraxa::bal_t(std::stoi(_json["value"].asString())),
        taraxa::val_t((_json["gas_price"].asString())),
        taraxa::val_t(_json["gas"].asString()),
        taraxa::addr_t(_json["to"].asString()), taraxa::sig_t(),
        taraxa::str2bytes(_json["data"].asString()));
    full_node->insertTransaction(trx);
    return toJS(trx.getHash());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_signTransaction(Json::Value const& _json) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_inspectTransaction(std::string const& _rlp) {
  return Json::Value();
}

string Taraxa::taraxa_sendRawTransaction(std::string const& _rlp) {
  auto tr_bytes = jsToBytes(_rlp, OnFailed::Throw);
  RLP const rlp(&tr_bytes);

  if (!rlp.isList())
    BOOST_THROW_EXCEPTION(InvalidTransactionFormat()
                          << errinfo_comment("transaction RLP must be a list"));

  auto nonce = rlp[0].toInt<u256>();
  auto gasPrice = rlp[1].toInt<u256>();
  auto gas = rlp[2].toInt<u256>();
  auto type = rlp[3].isEmpty() ? taraxa::Transaction::Type::Create
                               : taraxa::Transaction::Type::Call;
  auto receive_address =
      rlp[3].isEmpty() ? Address() : rlp[3].toHash<Address>(RLP::VeryStrict);
  auto value = rlp[4].toInt<u256>();

  if (!rlp[5].isData())
    BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment(
                              "transaction data RLP must be an array"));

  auto data = rlp[5].toBytes();

  int const v = rlp[6].toInt<int>();
  h256 const r = rlp[7].toInt<u256>();
  h256 const s = rlp[8].toInt<u256>();

  dev::SignatureStruct sig(r, s, v);
  if (rlp.itemCount() > 9)
    BOOST_THROW_EXCEPTION(InvalidTransactionFormat() << errinfo_comment(
                              "too many fields in the transaction RLP"));
  if (auto full_node = full_node_.lock()) {
    taraxa::Transaction trx(
        taraxa::trx_hash_t(30), taraxa::Transaction::Type::Call,
        taraxa::bal_t(nonce), taraxa::bal_t(value), taraxa::val_t(gasPrice),
        taraxa::val_t(gas), taraxa::addr_t(receive_address), sig, data);
    full_node->insertTransaction(trx);
    return toJS(trx.getHash());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

string Taraxa::taraxa_call(Json::Value const& _json,
                           string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_estimateGas(Json::Value const& _json) {
  //Dummy data
  return toJS("0x5208");
}

bool Taraxa::taraxa_flush() { return false; }

Json::Value Taraxa::taraxa_getBlockByHash(string const& _blockHash,
                                          bool _includeTransactions) {
  if (auto full_node = full_node_.lock()) {
    Json::Value res;
    auto block = full_node->getDagBlock(taraxa::blk_hash_t(_blockHash));
    res["hash"] = toJS(block->getHash());
    res["parentHash"] = toJS(block->getPivot());
    res["sha3Uncles"] = "";
    res["author"] = "";
    res["stateRoot"] = "";
    res["transactionsRoot"] = "";
    res["receiptsRoot"] = "";
    res["number"] = toJS(block->getLevel());
    res["gasUsed"] = "";   // TODO calculate gasUsed
    res["gasLimit"] = "";  // TODO calculate gasLimit
    res["extraData"] = "";
    res["logsBloom"] = "";
    res["timestamp"] = "";
    res["author"] = "";
    res["nonce"] = "";
    res["sha3Uncles"] = "";
    res["difficulty"] = "";
    res["totalDifficulty"] = "";
    res["size"] = "";  // Get total size of blocks
    res["uncles"] = "";
    res["transactions"] = Json::Value(Json::arrayValue);
    for (unsigned i = 0; i < block->getTrxs().size(); i++) {
      auto _t = full_node->getTransaction(block->getTrxs()[i]);
      if (_includeTransactions) {
        Json::Value tr_js = dev::toJson(_t);
        tr_js["blockHash"] = toJS(block->getHash());
        tr_js["transactionIndex"] = toJS(i);
        tr_js["blockNumber"] = toJS(block->getLevel());
        res["transactions"].append(tr_js);
      } else {
        res["transactions"].append(toJS(_t->getHash()));
      }
    }
    return res;
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getBlockByNumber(string const& _blockNumber,
                                            bool _includeTransactions) {
  if (auto full_node = full_node_.lock()) {
    Json::Value res;
    auto blocks =
        full_node->getDagBlocksAtLevel(std::stoi(_blockNumber, 0, 16), 1);
    if (blocks.size() == 0) return res;
    auto block = blocks[0];
    res["hash"] = toJS(block->getHash());
    res["parentHash"] = toJS(block->getPivot());
    res["sha3Uncles"] = "";
    res["author"] = "";
    res["stateRoot"] = "";
    res["transactionsRoot"] = "";
    res["receiptsRoot"] = "";
    res["number"] = toJS(block->getLevel());
    res["gasUsed"] = "";   // TODO calculate gasUsed
    res["gasLimit"] = "";  // TODO calculate gasLimit
    res["extraData"] = "";
    res["logsBloom"] = "";
    res["timestamp"] = "";
    res["author"] = "";
    res["nonce"] = "";
    res["sha3Uncles"] = "";
    res["difficulty"] = "";
    res["totalDifficulty"] = "";
    res["size"] = "";  // Get total size of blocks
    res["uncles"] = "";
    res["transactions"] = Json::Value(Json::arrayValue);
    for (unsigned i = 0; i < block->getTrxs().size(); i++) {
      auto _t = full_node->getTransaction(block->getTrxs()[i]);
      if (_includeTransactions) {
        Json::Value tr_js = dev::toJson(_t);
        tr_js["blockHash"] = toJS(block->getHash());
        tr_js["transactionIndex"] = toJS(i);
        tr_js["blockNumber"] = toJS(block->getLevel());
        res["transactions"].append(tr_js);
      } else {
        res["transactions"].append(toJS(_t->getHash()));
      }
    }
    return res;
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getTransactionByHash(
    string const& _transactionHash) {
  if (auto full_node = full_node_.lock()) {
    Json::Value res;
    auto trx = full_node->getTransaction(taraxa::trx_hash_t(_transactionHash));
    if (trx) return toJson(trx);
    return Json::Value();
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getTransactionByBlockHashAndIndex(
    string const& _blockHash, string const& _transactionIndex) {
  if (auto full_node = full_node_.lock()) {
    Json::Value res;
    auto block = full_node->getDagBlock(taraxa::blk_hash_t(_blockHash));
    auto trxs = block->getTrxs();
    if (trxs.size() > std::stoi(_transactionIndex)) {
      auto t = full_node->getTransaction(trxs[std::stoi(_transactionIndex)]);
      if (t) return toJson(t);
    }
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getTransactionByBlockNumberAndIndex(
    string const& _blockNumber, string const& _transactionIndex) {
  if (auto full_node = full_node_.lock()) {
    Json::Value res;
    auto blocks = full_node->getDagBlocksAtLevel(std::stoi(_blockNumber), 1);
    if (blocks.size() > 0) {
      auto trxs = blocks[0]->getTrxs();
      if (trxs.size() > std::stoi(_transactionIndex)) {
        auto t = full_node->getTransaction(trxs[std::stoi(_transactionIndex)]);
        if (t) return toJson(t);
      }
    }
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getTransactionReceipt(
    string const& _transactionHash) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getUncleByBlockHashAndIndex(
    string const& _blockHash, string const& _uncleIndex) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getUncleByBlockNumberAndIndex(
    string const& _blockNumber, string const& _uncleIndex) {
  return Json::Value();
}

string Taraxa::taraxa_newFilter(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newFilterEx(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newBlockFilter() { return ""; }

string Taraxa::taraxa_newPendingTransactionFilter() { return ""; }

bool Taraxa::taraxa_uninstallFilter(string const& _filterId) { return false; }

Json::Value Taraxa::taraxa_getFilterChanges(string const& _filterId) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getFilterChangesEx(string const& _filterId) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getFilterLogs(string const& _filterId) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getFilterLogsEx(string const& _filterId) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getLogs(Json::Value const& _json) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getLogsEx(Json::Value const& _json) {
  return Json::Value();
}

Json::Value Taraxa::taraxa_getWork() { return Json::Value(); }

Json::Value Taraxa::taraxa_syncing() { return Json::Value(); }

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
  return Json::Value();
}
