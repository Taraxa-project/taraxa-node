#include "Eth.h"

#include <json/json.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::net {
using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::final_chain;
using namespace ::taraxa::state_api;

// TODO REMOVE
DEV_SIMPLE_EXCEPTION(InvalidAddress);
/// Convert the given string into an address.
inline dev::Address toAddress(std::string const& _s) {
  try {
    auto b = fromHex(_s.substr(0, 2) == "0x" ? _s.substr(2) : _s, dev::WhenError::Throw);
    if (b.size() == 20) return Address(b);
  } catch (dev::BadHexCharacter&) {
  }
  BOOST_THROW_EXCEPTION(InvalidAddress());
}

template <typename... T>
Json::Value toJson(T const&... t) {
  return dev::toJS(t...);
}

template <typename T>
Json::Value toJson(std::vector<T> const& _es) {
  Json::Value res(Json::arrayValue);
  for (auto const& e : _es) {
    res.append(toJson(e));
  }
  return res;
}

template <typename T>
Json::Value toJson(std::optional<T> const& t) {
  return t ? toJson(*t) : Json::Value(Json::nullValue);
}

void add(Json::Value& obj, ExtendedTransactionLocation const& info, bool with_hash = true) {
  obj["blockNumber"] = toJson(info.blk_n);
  obj["blockHash"] = toJson(info.blk_h);
  obj["transactionIndex"] = toJson(info.index);
  if (with_hash) {
    obj["transactionHash"] = toJson(info.trx_hash);
  }
}

Json::Value toJson(Transaction const& trx, ExtendedTransactionLocation const& loc) {
  Json::Value res(Json::objectValue);
  add(res, loc, false);
  res["hash"] = toJson(trx.getHash());
  res["input"] = toJson(trx.getData());
  res["to"] = toJson(trx.getReceiver());
  res["from"] = toJson(trx.getSender());
  res["gas"] = toJson(trx.getGas());
  res["gasPrice"] = toJson(trx.getGasPrice());
  res["nonce"] = toJson(trx.getNonce());
  res["value"] = toJson(trx.getValue());
  auto const& vrs = trx.getVRS();
  res["r"] = toJson(vrs.r);
  res["s"] = toJson(vrs.s);
  res["v"] = toJson(vrs.v);
  return res;
}

Json::Value toJson(LocalisedTransaction const& lt) { return toJson(lt.trx, lt.trx_loc); }

Json::Value toJson(BlockHeader const& obj) {
  Json::Value res(Json::objectValue);
  res["parentHash"] = toJson(obj.parent_hash);
  res["sha3Uncles"] = toJson(BlockHeader::uncles());
  res["stateRoot"] = toJson(obj.state_root);
  res["transactionsRoot"] = toJson(obj.transactions_root);
  res["receiptsRoot"] = toJson(obj.receipts_root);
  res["number"] = toJson(obj.number);
  res["gasUsed"] = toJson(obj.gas_used);
  res["gasLimit"] = toJson(obj.gas_limit);
  res["extraData"] = toJson(obj.extra_data);
  res["logsBloom"] = toJson(obj.log_bloom);
  res["timestamp"] = toJson(obj.timestamp);
  res["author"] = res["miner"] = toJson(obj.author);
  res["mixHash"] = toJson(BlockHeader::mix_hash());
  res["nonce"] = toJson(BlockHeader::nonce());
  res["uncles"] = Json::Value(Json::arrayValue);

  res["hash"] = toJson(obj.hash);
  res["size"] = toJson(obj.ethereum_rlp_size);
  res["totalDifficulty"] = "0x0";
  return res;
}

Json::Value toJson(BlockHeaderWithTransactions const& obj) {
  Json::Value res = toJson(obj.h);
  auto& trxs_json = res["transactions"] = Json::Value(Json::arrayValue);
  if (obj.trxs.index() == 0) {
    trxs_json = toJson(get<0>(obj.trxs));
  } else {
    ExtendedTransactionLocation loc;
    loc.blk_n = obj.number;
    loc.blk_h = obj.hash;
    for (auto const& t : get<1>(obj.trxs)) {
      trxs_json.append(toJson(t, loc));
      ++loc.index;
    }
  }
  return res;
}

Json::Value toJson(LocalisedLogEntry const& lle) {
  Json::Value res(Json::objectValue);
  add(res, lle.trx_loc);
  res["removed"] = false;
  res["data"] = toJson(lle.le.data);
  res["address"] = toJson(lle.le.address);
  res["logIndex"] = toJson(lle.position_in_receipt);
  auto& topics_json = res["topics"] = Json::Value(Json::arrayValue);
  for (auto const& t : lle.le.topics) {
    topics_json.append(toJson(t));
  }
  return res;
}

Json::Value toJson(LocalisedTransactionReceipt const& ltr) {
  Json::Value res(Json::objectValue);
  add(res, ltr.trx_loc);
  res["from"] = toJson(ltr.trx_from);
  res["to"] = toJson(ltr.trx_to);
  res["status"] = toString(ltr.r.status_code);
  res["gasUsed"] = toJson(ltr.r.gas_used);
  res["cumulativeGasUsed"] = toJson(ltr.r.cumulative_gas_used);
  res["contractAddress"] = toJson(ltr.r.contract_address);
  res["logsBloom"] = toJson(ltr.r.bloom());
  auto& logs_json = res["logs"] = Json::Value(Json::arrayValue);
  uint log_i = 0;
  for (auto const& le : ltr.r.logs) {
    logs_json.append(toJson(LocalisedLogEntry{le, ltr.trx_loc, log_i++}));
  }
  return res;
}

TransactionSkeleton toTransactionSkeleton(Json::Value const& _json) {
  TransactionSkeleton ret;
  if (!_json.isObject() || _json.empty()) {
    return ret;
  }
  if (!_json["from"].empty()) {
    ret.from = toAddress(_json["from"].asString());
  }
  if (!_json["to"].empty() && _json["to"].asString() != "0x" && !_json["to"].asString().empty()) {
    ret.to = toAddress(_json["to"].asString());
  }
  if (!_json["value"].empty()) {
    ret.value = jsToU256(_json["value"].asString());
  }
  if (!_json["gas"].empty()) {
    ret.gas = jsToInt(_json["gas"].asString());
  }
  if (!_json["gasPrice"].empty()) {
    ret.gas_price = jsToU256(_json["gasPrice"].asString());
  }
  if (!_json["data"].empty()) {
    ret.data = jsToBytes(_json["data"].asString(), OnFailed::Throw);
  }
  if (!_json["code"].empty()) {
    ret.data = jsToBytes(_json["code"].asString(), OnFailed::Throw);
  }
  if (!_json["nonce"].empty()) {
    ret.nonce = jsToInt(_json["nonce"].asString());
  }
  return ret;
}

struct Filters {
  using FilterID = uint64_t;

  optional<LogFilter> getLogFilter(FilterID id) const { return {}; }
  FilterID newBlockFilter() { return 0; }
  FilterID newPendingTransactionFilter() { return 0; }
  FilterID newLogFilter(LogFilter&& _filter) { return 0; }
  bool uninstallFilter(FilterID id) { return false; }

  void note_block(h256 const& blk_hash) {}
  void note_pending_transactions(RangeView<h256> const& trx_hashes) {}
  void note_receipts(RangeView<TransactionReceipt> const& receipts) {}

  Json::Value poll(FilterID id) { return Json::Value(Json::arrayValue); }
};

struct EthImpl : Eth, EthParams {
  Filters filters;

  EthImpl(EthParams&& prerequisites) : EthParams(move(prerequisites)) {}

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"eth", "1.0"}}; }

  string eth_protocolVersion() override { return toJS(63); }

  string eth_coinbase() override { return toJS(address); }

  string eth_gasPrice() override { return toJS(gas_pricer()); }

  Json::Value eth_accounts() override { return toJson(vector{address}); }

  string eth_blockNumber() override { return toJS(final_chain->last_block_number()); }

  string eth_getBalance(string const& _address, string const& _blockNumber) override {
    return toJS(
        final_chain->get_account(toAddress(_address), parse_blk_num(_blockNumber)).value_or(ZeroAccount).Balance);
  }

  string eth_getStorageAt(string const& _address, string const& _position, string const& _blockNumber) override {
    return toJS(
        final_chain->get_account_storage(toAddress(_address), jsToU256(_position), parse_blk_num(_blockNumber)));
  }

  string eth_getStorageRoot(string const& _address, string const& _blockNumber) override {
    return toJS(final_chain->get_account(toAddress(_address), parse_blk_num(_blockNumber))
                    .value_or(ZeroAccount)
                    .storage_root_eth());
  }

  string eth_getCode(string const& _address, string const& _blockNumber) override {
    return toJS(final_chain->get_code(toAddress(_address), parse_blk_num(_blockNumber)));
  }

  string eth_call(Json::Value const& _json, string const& _blockNumber) override {
    auto t = toTransactionSkeleton(_json);
    auto blk_n = parse_blk_num(_blockNumber);
    populateTransactionWithDefaults(t, blk_n);
    return toJS(call(blk_n, t, false).CodeRet);
  }

  string eth_estimateGas(Json::Value const& _json) override {
    auto t = toTransactionSkeleton(_json);
    // TODO WTF?
    auto blk_n = parse_blk_num("latest");
    populateTransactionWithDefaults(t, blk_n);
    return toJS(call(blk_n, t, true).GasUsed);
  }

  string eth_getTransactionCount(string const& _address, string const& _blockNumber) override {
    return toJS(transaction_count(parse_blk_num(_blockNumber), toAddress(_address)));
  }

  Json::Value eth_getBlockTransactionCountByHash(string const& _blockHash) override {
    return toJson(final_chain->transactionCount(jsToFixed<32>(_blockHash)));
  }

  Json::Value eth_getBlockTransactionCountByNumber(string const& _blockNumber) override {
    return toJS(final_chain->transactionCount(parse_blk_num(_blockNumber)));
  }

  Json::Value eth_getUncleCountByBlockHash(string const&) override { return toJS(0); }

  Json::Value eth_getUncleCountByBlockNumber(string const&) override { return toJS(0); }

  string eth_sendTransaction(Json::Value const& _json) override {
    auto t = toTransactionSkeleton(_json);
    populateTransactionWithDefaults(t, final_chain->last_block_number());
    Transaction trx(t.nonce.value_or(0), t.value, t.gas_price.value_or(0), t.gas.value_or(0), t.data, secret,
                    t.to ? optional(t.to) : std::nullopt, chain_id);
    trx.rlp(true);
    return toJS(send_trx(trx));
  }

  string eth_sendRawTransaction(std::string const& _rlp) override {
    return toJS(send_trx(Transaction(jsToBytes(_rlp, OnFailed::Throw), true)));
  }

  Json::Value eth_getBlockByHash(string const& _blockHash, bool _includeTransactions) override {
    return toJson(final_chain->blockHeaderWithTransactions(jsToFixed<32>(_blockHash), _includeTransactions));
  }

  Json::Value eth_getBlockByNumber(string const& _blockNumber, bool _includeTransactions) override {
    return toJson(final_chain->blockHeaderWithTransactions(parse_blk_num(_blockNumber), _includeTransactions));
  }

  Json::Value eth_getTransactionByHash(string const& _transactionHash) override {
    return toJson(final_chain->localisedTransaction(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getTransactionByBlockHashAndIndex(string const& _blockHash,
                                                    string const& _transactionIndex) override {
    return toJson(final_chain->localisedTransaction(jsToFixed<32>(_blockHash), jsToInt(_transactionIndex)));
  }

  Json::Value eth_getTransactionByBlockNumberAndIndex(string const& _blockNumber,
                                                      string const& _transactionIndex) override {
    return toJson(final_chain->localisedTransaction(jsToInt(_transactionIndex), parse_blk_num(_blockNumber)));
  }

  Json::Value eth_getTransactionReceipt(string const& _transactionHash) override {
    return toJson(final_chain->localisedTransactionReceipt(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getUncleByBlockHashAndIndex(string const&, string const&) override {
    return Json::Value(Json::nullValue);
  }

  Json::Value eth_getUncleByBlockNumberAndIndex(string const&, string const&) override {
    return Json::Value(Json::nullValue);
  }

  string eth_newFilter(Json::Value const& _json) override { return toJS(filters.newLogFilter(toLogFilter(_json))); }

  string eth_newBlockFilter() override { return toJS(filters.newBlockFilter()); }

  string eth_newPendingTransactionFilter() override { return toJS(filters.newPendingTransactionFilter()); }

  bool eth_uninstallFilter(string const& _filterId) override { return filters.uninstallFilter(jsToInt(_filterId)); }

  Json::Value eth_getFilterChanges(string const& _filterId) override { return filters.poll(jsToInt(_filterId)); }

  Json::Value eth_getFilterLogs(string const& _filterId) override {
    if (auto filter = filters.getLogFilter(jsToInt(_filterId))) {
      return toJson(final_chain->logs(*filter));
    }
    return Json::Value(Json::nullValue);
  }

  Json::Value eth_getLogs(Json::Value const& _json) override { return toJson(final_chain->logs(toLogFilter(_json))); }

  Json::Value eth_syncing() override {
    // TODO
    return Json::Value(false);
  }

  Json::Value eth_chainId() override { return chain_id ? Json::Value(toJS(chain_id)) : Json::Value(Json::nullValue); }

  void note_block(h256 const& blk_hash) override { filters.note_block(blk_hash); }

  void note_pending_transactions(RangeView<h256> const& trx_hashes) override {
    filters.note_pending_transactions(trx_hashes);
  }

  void note_receipts(RangeView<TransactionReceipt> const& receipts) override { filters.note_receipts(receipts); }

  uint64_t transaction_count(BlockNumber n, Address const& addr) {
    return final_chain->get_account(addr, n).value_or(ZeroAccount).Nonce;
  }

  void populateTransactionWithDefaults(TransactionSkeleton& t, BlockNumber blk_n) {
    if (!t.from) {
      t.from = address;
    }
    if (!t.nonce) {
      t.nonce = transaction_count(blk_n, t.from);
    }
    if (!t.gas_price) {
      t.gas_price = gas_pricer();
    }
    if (!t.gas) {
      t.gas = 90000;
    }
  }

  BlockNumber parse_blk_num(string const& json_str, std::optional<BlockNumber> latest_block = {}) {
    if (json_str == "latest" || json_str == "pending") {
      return latest_block ? *latest_block : final_chain->last_block_number();
    }
    return json_str == "earliest" ? 0 : jsToInt(json_str);
  }

  LogFilter toLogFilter(Json::Value const& json) {
    auto latest_block = final_chain->last_block_number();
    LogFilter filter(latest_block, latest_block);
    if (!json.isObject() || json.empty()) {
      return filter;
    }
    // check only !empty. it should throw exceptions if input params are
    // incorrect
    if (auto const& fromBlock = json["fromBlock"]; !fromBlock.empty()) {
      filter.withEarliest(parse_blk_num(fromBlock.asString(), latest_block));
    }
    if (auto const& toBlock = json["toBlock"]; !toBlock.empty()) {
      filter.withLatest(parse_blk_num(toBlock.asString(), latest_block));
    }
    if (auto const& address = json["address"]; !address.empty()) {
      if (address.isArray()) {
        for (auto const& i : address) {
          filter.address(toAddress(i.asString()));
        }
      } else {
        filter.address(toAddress(address.asString()));
      }
    }
    if (auto const& topics = json["topics"]; !topics.empty()) {
      for (unsigned i = 0; i < topics.size(); i++) {
        auto const& topic = topics[i];
        if (topic.isArray()) {
          for (auto const& t : topic) {
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

  state_api::ExecutionResult call(BlockNumber blk_n, TransactionSkeleton const& trx, bool free_gas) {
    return final_chain->call(
        {
            trx.from,
            trx.gas_price.value_or(0),
            trx.to,
            trx.nonce.value_or(0),
            trx.value,
            trx.gas.value_or(0),
            trx.data,
        },
        blk_n,
        state_api::ExecutionOptions{
            true,
            free_gas,
        });
  }
};

Eth* NewEth(EthParams&& prerequisites) { return new EthImpl(move(prerequisites)); }

}  // namespace taraxa::net