#include "Taraxa.h"
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libethcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libwebthree/WebThree.h>
#include <csignal>
#include "AccountHolder.h"

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace shh;
using namespace dev::rpc;

Taraxa::Taraxa(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

string Taraxa::taraxa_protocolVersion() { return ""; }

string Taraxa::taraxa_coinbase() { return ""; }

string Taraxa::taraxa_hashrate() { return ""; }

bool Taraxa::taraxa_mining() { return false; }

string Taraxa::taraxa_gasPrice() { return ""; }

Json::Value Taraxa::taraxa_accounts() { return ""; }

string Taraxa::taraxa_blockNumber() {
  if (auto full_node = full_node_.lock()) {
    return toJS(full_node->getDagMaxLevel());
  }
}

string Taraxa::taraxa_getBalance(string const& _address,
                                 string const& _blockNumber) {
  return "";
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

Json::Value Taraxa::taraxa_pendingTransactions() { return ""; }

string Taraxa::taraxa_getTransactionCount(string const& _address,
                                          string const& _blockNumber) {
  return "";
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByHash(
    string const& _blockHash) {
  return "";
}

Json::Value Taraxa::taraxa_getBlockTransactionCountByNumber(
    string const& _blockNumber) {
  return "";
}

Json::Value Taraxa::taraxa_getUncleCountByBlockHash(string const& _blockHash) {
  return "";
}

Json::Value Taraxa::taraxa_getUncleCountByBlockNumber(
    string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_getCode(string const& _address,
                              string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_sendTransaction(Json::Value const& _json) { return ""; }

Json::Value Taraxa::taraxa_signTransaction(Json::Value const& _json) {
  return "";
}

Json::Value Taraxa::taraxa_inspectTransaction(std::string const& _rlp) {
  return "";
}

string Taraxa::taraxa_sendRawTransaction(std::string const& _rlp) { return ""; }

string Taraxa::taraxa_call(Json::Value const& _json,
                           string const& _blockNumber) {
  return "";
}

string Taraxa::taraxa_estimateGas(Json::Value const& _json) { return ""; }

bool Taraxa::taraxa_flush() { return false; }

Json::Value Taraxa::taraxa_getBlockByHash(string const& _blockHash,
                                          bool _includeTransactions) {
  return "";
}

Json::Value Taraxa::taraxa_getBlockByNumber(string const& _blockNumber,
                                            bool _includeTransactions) {
  // if (auto full_node = full_node_.lock()) {
  //   return toJS(full_node->getDagBlocksAtLevel(std::stoi(_blockNumber), 1));
  // }
  return "";
}

Json::Value Taraxa::taraxa_getTransactionByHash(
    string const& _transactionHash) {
  return "";
}

Json::Value Taraxa::taraxa_getTransactionByBlockHashAndIndex(
    string const& _blockHash, string const& _transactionIndex) {
  return "";
}

Json::Value Taraxa::taraxa_getTransactionByBlockNumberAndIndex(
    string const& _blockNumber, string const& _transactionIndex) {
  return "";
}

Json::Value Taraxa::taraxa_getTransactionReceipt(
    string const& _transactionHash) {
  return "";
}

Json::Value Taraxa::taraxa_getUncleByBlockHashAndIndex(
    string const& _blockHash, string const& _uncleIndex) {
  return "";
}

Json::Value Taraxa::taraxa_getUncleByBlockNumberAndIndex(
    string const& _blockNumber, string const& _uncleIndex) {
  return "";
}

string Taraxa::taraxa_newFilter(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newFilterEx(Json::Value const& _json) { return ""; }

string Taraxa::taraxa_newBlockFilter() { return ""; }

string Taraxa::taraxa_newPendingTransactionFilter() { return ""; }

bool Taraxa::taraxa_uninstallFilter(string const& _filterId) { return false; }

Json::Value Taraxa::taraxa_getFilterChanges(string const& _filterId) {
  return "";
}

Json::Value Taraxa::taraxa_getFilterChangesEx(string const& _filterId) {
  return "";
}

Json::Value Taraxa::taraxa_getFilterLogs(string const& _filterId) { return ""; }

Json::Value Taraxa::taraxa_getFilterLogsEx(string const& _filterId) {
  return "";
}

Json::Value Taraxa::taraxa_getLogs(Json::Value const& _json) { return ""; }

Json::Value Taraxa::taraxa_getLogsEx(Json::Value const& _json) { return ""; }

Json::Value Taraxa::taraxa_getWork() { return ""; }

Json::Value Taraxa::taraxa_syncing() { return ""; }

string Taraxa::taraxa_chainId() { return ""; }

bool Taraxa::taraxa_submitWork(string const& _nonce, string const&,
                               string const& _mixHash) {
  return "";
}

bool Taraxa::taraxa_submitHashrate(string const& _hashes, string const& _id) {
  return "";
}

string Taraxa::taraxa_register(string const& _address) { return ""; }

bool Taraxa::taraxa_unregister(string const& _accountId) { return false; }

Json::Value Taraxa::taraxa_fetchQueuedTransactions(string const& _accountId) {
  return "";
}
