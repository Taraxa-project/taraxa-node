#include "TaraxaVM.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <utility>
#include "util.hpp"

using namespace std;
using namespace boost::property_tree;
using namespace dev;

namespace taraxa::vm {

ptree EthTransactionReceipt::toPtree() const { return ptree(); }

void EthTransactionReceipt::fromPtree(const ptree& p) {
  const auto& rootOpt = p.get_optional<string>("root");
  if (rootOpt.has_value()) {
    root.emplace(fromHex(rootOpt.get()));
  }
  const auto& statusOpt =
      p.get_optional<decltype(status)::value_type>("status");
  if (statusOpt.has_value()) {
    status.emplace(statusOpt.get());
  }
  cumulativeGasUsed =
      decltype(cumulativeGasUsed)(p.get<string>("cumulativeGasUsed"));
  logsBloom = p.get<decltype(logsBloom)>("logsBloom");
  for (auto& logPtreeEntry : p.get_child("logs")) {
    LogEntry logEntry;
    logEntry.fromPtree(logPtreeEntry.second);
    logs.push_back(logEntry);
  }
  transactionHash = p.get<decltype(transactionHash)>("transactionHash");
  contractAddress = p.get<decltype(contractAddress)>("contractAddress");
  gasUsed = decltype(gasUsed)(p.get<string>("gasUsed"));
}

ptree LogEntry::toPtree() const { return ptree(); }

void LogEntry::fromPtree(const ptree& p) {
  address = p.get<decltype(address)>("address");
  for (auto& topicPtree : p.get_child("topics")) {
    topics.push_back(
        topicPtree.second.get_value<decltype(topics)::value_type>());
  }
  data = fromHex(p.get<string>("data"));
  blockNumber = p.get<decltype(blockNumber)>("blockNumber");
  transactionHash = p.get<decltype(transactionHash)>("transactionHash");
  transactionIndex = p.get<decltype(transactionIndex)>("transactionIndex");
  blockHash = p.get<decltype(blockHash)>("blockHash");
  logIndex = p.get<decltype(logIndex)>("logIndex");
  removed = p.get<decltype(removed)>("removed");
}

ptree TaraxaTransactionReceipt::toPtree() const { return ptree(); }

void TaraxaTransactionReceipt::fromPtree(const ptree& p) {
  returnValue = fromHex(p.get<string>("returnValue"));
  ethereumReceipt.fromPtree(p.get_child("ethereumReceipt"));
  contractError = p.get<decltype(contractError)>("contractError");
}

ptree StateTransitionResult::toPtree() const { return ptree(); }

void StateTransitionResult::fromPtree(const ptree& p) {
  stateRoot = p.get<decltype(stateRoot)>("stateRoot");
  for (auto& receiptPtreeEntry : p.get_child("receipts")) {
    TaraxaTransactionReceipt receipt;
    receipt.fromPtree(receiptPtreeEntry.second);
    receipts.push_back(receipt);
  }
  for (auto& logPtreeEntry : p.get_child("allLogs")) {
    LogEntry logEntry;
    logEntry.fromPtree(logPtreeEntry.second);
    allLogs.push_back(logEntry);
  }
  usedGas = decltype(usedGas)(p.get<string>("usedGas"));
}

ptree Transaction::toPtree() const {
  ptree ret;
  if (to.has_value()) {
    ret.put("to", toHexPrefixed(to.value()));
  }
  ret.put("from", toHexPrefixed(from));
  ret.put("nonce", nonce);
  ret.put("amount", amount);
  ret.put("gasLimit", gasLimit);
  ret.put("gasPrice", gasPrice);
  ret.put("data", toHexPrefixed(data));
  ret.put("hash", toHexPrefixed(hash));
  return ret;
}

void Transaction::fromPtree(const ptree& ptree) {}

ptree Block::toPtree() const {
  ptree ret;
  ret.put("coinbase", toHexPrefixed(coinbase));
  ret.put("number", number);
  ret.put("difficulty", difficulty);
  ret.put("time", time);
  ret.put("gasLimit", gasLimit);
  ret.put("hash", toHexPrefixed(hash));
  ptree transactionsPtree;
  for (auto& tx : transactions) {
    append(transactionsPtree, tx.toPtree());
  }
  ret.put_child("transactions", transactionsPtree);
  return ret;
}

void Block::fromPtree(const ptree& ptree) {}

ptree StateTransitionRequest::toPtree() const {
  ptree ret;
  ret.put("stateRoot", toHexPrefixed(stateRoot));
  ret.put_child("block", block.toPtree());
  return ret;
}

void StateTransitionRequest::fromPtree(const ptree& ptree) {}

ptree DBConfig::toPtree() const {
  ptree ret;
  ret.put("type", type);
  ret.put_child("options", options);
  return ret;
}

void DBConfig::fromPtree(const ptree& ptree) {}

ptree StateDBConfig::toPtree() const {
  ptree ret;
  ret.put_child("db", db.toPtree());
  ret.put("cacheSize", cacheSize);
  return ret;
}

void StateDBConfig::fromPtree(const ptree& ptree) {}

ptree VmConfig::toPtree() const {
  ptree ret;
  ret.put_child("readDB", readDB.toPtree());
  return ret;
}

void VmConfig::fromPtree(const ptree& ptree) {}

TaraxaVM::TaraxaVM(string goAddress) : goAddress(move(goAddress)) {}

shared_ptr<TaraxaVM> TaraxaVM::fromConfig(const VmConfig& config) {
  ptree argsPtree;
  append(argsPtree, config.toPtree());
  const auto& argsEncoded = toJsonArrayString(argsPtree);
  const auto& resultJson =
      Call(nullptr, cgo_str("NewVM"), cgo_str(argsEncoded));
  const auto& resultPtree = strToJson(resultJson);
  auto i = resultPtree.begin();
  const auto& goAddr = (i++)->second.get_value<string>();
  const auto& err = (i++)->second.get_value<string>();
  return make_shared<TaraxaVM>(goAddr);
}

StateTransitionResult TaraxaVM::transitionState(
    const StateTransitionRequest& req) {
  ptree argsPtree;
  append(argsPtree, req.toPtree());
  const auto& argsEncoded = toJsonArrayString(argsPtree);
  const auto& resultJson = Call(cgo_str(goAddress), cgo_str("RunLikeEthereum"),
                                cgo_str(argsEncoded));
  const auto& resultPtree = strToJson(resultJson);
  auto i = resultPtree.begin();
  const auto& result = (i++)->second;
  const auto& totalTime = (i++)->second.get_value<int64_t>();
  const auto& err = (i++)->second.get_value<string>();
  StateTransitionResult ret;
  ret.fromPtree(result);
  return ret;
}

}