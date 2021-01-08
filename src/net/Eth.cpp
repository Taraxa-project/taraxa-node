#include "Eth.h"

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>

#include "aleth/JsonHelper.h"

using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::aleth;
using namespace ::jsonrpc;

namespace taraxa::net {

string exceptionToErrorMessage() {
  try {
    throw;
  } catch (std::exception const& e) {
    return e.what();
  }
  return "Unknown exception.";
}

string Eth::eth_protocolVersion() { return toJS(c_protocolVersion); }

string Eth::eth_coinbase() { return toJS(node_api->address()); }

string Eth::eth_gasPrice() { return toJS(gas_pricer()); }

Json::Value Eth::eth_accounts() { return toJson(Addresses{node_api->address()}); }

string Eth::eth_blockNumber() { return toJS(chain_db->last_block_number()); }

string Eth::eth_getBalance(string const& _address, string const& _blockNumber) {
  try {
    return toJS(state_api->balanceAt(toAddress(_address), jsToBlockNumberPendingAsLatest(_blockNumber)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_getStorageAt(string const& _address, string const& _position, string const& _blockNumber) {
  try {
    return toJS(toCompactBigEndian(
        state_api->stateAt(toAddress(_address), jsToU256(_position), jsToBlockNumberPendingAsLatest(_blockNumber)),
        32));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_getStorageRoot(string const& _address, string const& _blockNumber) {
  try {
    return toString(state_api->stateRootAt(toAddress(_address), jsToBlockNumberPendingAsLatest(_blockNumber)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_getCode(string const& _address, string const& _blockNumber) {
  try {
    return toJS(state_api->codeAt(toAddress(_address), jsToBlockNumberPendingAsLatest(_blockNumber)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}
string Eth::eth_call(Json::Value const& _json, string const& _blockNumber) {
  try {
    auto t = toTransactionSkeleton(_json);
    auto blk_n = jsToBlockNumberPendingAsLatest(_blockNumber);
    populateTransactionWithDefaults(t, blk_n);
    return toJS(state_api->call(blk_n, t));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_estimateGas(Json::Value const& _json) {
  try {
    auto t = toTransactionSkeleton(_json);
    auto blk_n = jsToBlockNumberPendingAsLatest("latest");
    populateTransactionWithDefaults(t, blk_n);
    return toJS(state_api->estimateGas(blk_n, t));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_getTransactionCount(string const& _address, string const& _blockNumber) {
  try {
    return toJS(transaction_count(jsToBlockNumber(_blockNumber), toAddress(_address)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_pendingTransactions() { return toJson(pending_block->transactions()); }

Json::Value Eth::eth_getBlockTransactionCountByHash(string const& _blockHash) {
  try {
    h256 blockHash = jsToFixed<32>(_blockHash);
    if (!chain_db->isKnown(blockHash)) return Json::Value(Json::nullValue);
    return toJS(chain_db->transactionCount(blockHash));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getBlockTransactionCountByNumber(string const& _blockNumber) {
  try {
    if (auto blockNumber = jsToBlockNumber(_blockNumber)) {
      if (!chain_db->isKnown(*blockNumber)) {
        return Json::Value(Json::nullValue);
      }
      return toJS(chain_db->transactionCount(*blockNumber));
    }
    return toJS(pending_block->transactionsCount());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getUncleCountByBlockHash(string const&) { return toJS(0); }

Json::Value Eth::eth_getUncleCountByBlockNumber(string const&) { return toJS(0); }

string Eth::eth_sendTransaction(Json::Value const& _json) {
  try {
    TransactionSkeleton t = toTransactionSkeleton(_json);
    populateTransactionWithDefaults(t);
    return toJS(node_api->importTransaction(t));
  } catch (...) {
    throw JsonRpcException(exceptionToErrorMessage());
  }
}

string Eth::eth_sendRawTransaction(std::string const& _rlp) {
  try {
    return toJS(node_api->importTransaction(jsToBytes(_rlp, OnFailed::Throw)));
  } catch (...) {
    throw JsonRpcException(exceptionToErrorMessage());
  }
}

Json::Value Eth::eth_getBlockByHash(string const& _blockHash, bool _includeTransactions) {
  try {
    h256 h = jsToFixed<32>(_blockHash);
    if (!chain_db->isKnown(h)) return Json::Value(Json::nullValue);
    if (_includeTransactions)
      return toJson(chain_db->blockHeader(h), chain_db->blockDetails(h), {}, chain_db->transactions(h));
    else
      return toJson(chain_db->blockHeader(h), chain_db->blockDetails(h), {}, chain_db->transactionHashes(h));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getBlockByNumber(string const& _blockNumber, bool _includeTransactions) {
  try {
    if (auto n = jsToBlockNumber(_blockNumber)) {
      if (!chain_db->isKnown(*n)) {
        return Json::Value(Json::nullValue);
      }
      if (_includeTransactions) {
        return toJson(chain_db->blockHeader(*n), chain_db->blockDetails(*n), {}, chain_db->transactions(*n));
      }
      return toJson(chain_db->blockHeader(*n), chain_db->blockDetails(*n), {}, chain_db->transactionHashes(*n));
    }
    if (_includeTransactions) {
      return toJson(pending_block->header(), pending_block->details(), {}, pending_block->transactions());
    }
    return toJson(pending_block->header(), pending_block->details(), {}, pending_block->transactionHashes());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getTransactionByHash(string const& _transactionHash) {
  try {
    h256 h = jsToFixed<32>(_transactionHash);
    if (!chain_db->isKnownTransaction(h)) return Json::Value(Json::nullValue);
    return toJson(chain_db->localisedTransaction(h));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getTransactionByBlockHashAndIndex(string const& _blockHash, string const& _transactionIndex) {
  try {
    h256 bh = jsToFixed<32>(_blockHash);
    unsigned ti = jsToInt(_transactionIndex);
    if (!chain_db->isKnownTransaction(bh, ti)) return Json::Value(Json::nullValue);
    return toJson(chain_db->localisedTransaction(bh, ti));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getTransactionByBlockNumberAndIndex(string const& _blockNumber, string const& _transactionIndex) {
  try {
    auto ti = jsToInt(_transactionIndex);
    if (auto bn = jsToBlockNumber(_blockNumber)) {
      if (!chain_db->isKnownTransaction(*bn, ti)) {
        return Json::Value(Json::nullValue);
      }
      return toJson(chain_db->localisedTransaction(*bn, ti));
    }
    return toJson(pending_block->transaction(ti));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getTransactionReceipt(string const& _transactionHash) {
  try {
    h256 h = jsToFixed<32>(_transactionHash);
    if (!chain_db->isKnownTransaction(h)) return Json::Value(Json::nullValue);
    return toJson(chain_db->localisedTransactionReceipt(h));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getUncleByBlockHashAndIndex(string const&, string const&) { return Json::Value(Json::nullValue); }

Json::Value Eth::eth_getUncleByBlockNumberAndIndex(string const&, string const&) {
  return Json::Value(Json::nullValue);
}

string Eth::eth_newFilter(Json::Value const& _json) {
  try {
    return toJS(filter_api->newLogFilter(toLogFilter(_json)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Eth::eth_newBlockFilter() { return toJS(filter_api->newBlockFilter()); }

string Eth::eth_newPendingTransactionFilter() { return toJS(filter_api->newPendingTransactionFilter()); }

bool Eth::eth_uninstallFilter(string const& _filterId) {
  try {
    return filter_api->uninstallFilter(jsToInt(_filterId));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getFilterChanges(string const& _filterId) {
  try {
    Json::Value ret(Json::arrayValue);
    filter_api->poll(jsToInt(_filterId), {
                                             [&](auto const& changes) { return ret = toJson(changes); },
                                             [&](auto const& changes) { return ret = toJson(changes); },
                                             [&](auto const& changes) { return ret = toJson(changes); },
                                         });
    return ret;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getFilterLogs(string const& _filterId) {
  try {
    if (auto filter = filter_api->getLogFilter(jsToInt(_filterId))) {
      return toJson(chain_db->logs(*filter));
    }
    return Json::Value(Json::nullValue);
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_getLogs(Json::Value const& _json) {
  try {
    return toJson(chain_db->logs(toLogFilter(_json)));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Eth::eth_syncing() {
  auto sync = node_api->syncStatus();
  if (sync.state == SyncState::Idle || !sync.majorSyncing) return Json::Value(false);
  Json::Value info(Json::objectValue);
  info["startingBlock"] = sync.startBlockNumber;
  info["highestBlock"] = sync.highestBlockNumber;
  info["currentBlock"] = sync.currentBlockNumber;
  return info;
}

Json::Value Eth::eth_chainId() {
  if (auto v = node_api->chain_id(); v) {
    return Json::Value(toJS(v));
  }
  return Json::Value(Json::nullValue);
}

uint64_t Eth::transaction_count(std::optional<BlockNumber> n, Address const& addr) {
  if (n) {
    return state_api->nonceAt(addr, *n);
  }
  return state_api->nonceAt(addr, chain_db->last_block_number()) + pending_block->transactionsCount(addr);
}

void Eth::populateTransactionWithDefaults(TransactionSkeleton& t, std::optional<BlockNumber> blk_n) {
  if (!t.from) {
    t.from = node_api->address();
  }
  if (!t.nonce) {
    t.nonce = transaction_count(blk_n, t.from);
  }
  if (!t.gasPrice) {
    t.gasPrice = gas_pricer();
  }
  if (!t.gas) {
    t.gas = 90000;
  }
}

std::optional<BlockNumber> Eth::jsToBlockNumber(string const& _js, std::optional<BlockNumber> latest_block) {
  if (_js == "earliest") {
    return 0;
  }
  if (_js == "latest") {
    return latest_block ? *latest_block : chain_db->last_block_number();
  }
  if (_js == "pending") {
    return std::nullopt;
  }
  return jsToInt(_js);
}

BlockNumber Eth::jsToBlockNumberPendingAsLatest(std::string const& js, std::optional<BlockNumber> latest_block) {
  if (auto ret = jsToBlockNumber(js, latest_block)) {
    return *ret;
  }
  return latest_block ? *latest_block : chain_db->last_block_number();
}

LogFilter Eth::toLogFilter(Json::Value const& _json) {
  auto latest_block = chain_db->last_block_number();
  LogFilter filter(latest_block, latest_block);
  if (!_json.isObject() || _json.empty()) {
    return filter;
  }
  // check only !empty. it should throw exceptions if input params are
  // incorrect
  if (auto const& fromBlock = _json["fromBlock"]; !fromBlock.empty()) {
    filter.withEarliest(jsToBlockNumberPendingAsLatest(fromBlock.asString(), latest_block));
  }
  if (auto const& toBlock = _json["toBlock"]; !toBlock.empty()) {
    filter.withLatest(jsToBlockNumberPendingAsLatest(toBlock.asString(), latest_block));
  }
  if (auto const& address = _json["address"]; !address.empty()) {
    if (address.isArray()) {
      for (auto i : address) {
        filter.address(toAddress(i.asString()));
      }
    } else {
      filter.address(toAddress(address.asString()));
    }
  }
  if (auto const& topics = _json["topics"]; !topics.empty()) {
    for (unsigned i = 0; i < topics.size(); i++) {
      auto const& topic = topics[i];
      if (topic.isArray()) {
        for (auto t : topic) {
          if (!t.isNull()) {
            filter.topic(i, jsToFixed<32>(t.asString()));
          }
        }
      } else if (!topic.isNull()) {
        // if it is anything else then string, it
        // should and will fail
        filter.topic(i, jsToFixed<32>(topic.asString()));
      }
    }
  }
  return filter;
}

}  // namespace taraxa::net