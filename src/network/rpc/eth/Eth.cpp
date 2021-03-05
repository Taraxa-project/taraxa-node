#include "Eth.h"

#include <json/json.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::net::rpc::eth {
using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::final_chain;
using namespace ::taraxa::state_api;

struct EthImpl : Eth, EthParams {
  struct TransactionLocationWithBlockHash : TransactionLocation {
    h256 blk_h;
  };

  struct LocalisedTransaction {
    Transaction trx;
    optional<TransactionLocationWithBlockHash> trx_loc;
  };

  struct ExtendedTransactionLocation : TransactionLocationWithBlockHash {
    h256 trx_hash;
  };

  struct LocalisedTransactionReceipt {
    TransactionReceipt r;
    ExtendedTransactionLocation trx_loc;
    addr_t trx_from;
    optional<addr_t> trx_to;
  };

  struct LocalisedLogEntry {
    LogEntry le;
    ExtendedTransactionLocation trx_loc;
    uint64_t position_in_receipt = 0;
  };

  struct TransactionSkeleton {
    Address from;
    u256 value;
    bytes data;
    optional<Address> to;
    optional<uint64_t> nonce;
    optional<uint64_t> gas;
    optional<u256> gas_price;
  };

  struct LogFilter {
    BlockNumber from_block = 0;
    BlockNumber to_block = 0;
    AddressSet addresses;
    array<unordered_set<h256>, 4> topics;
  };

  struct FilterAPI {
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
  } filters;

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
    // TODO What is this?
    auto blk_n = parse_blk_num("latest");
    populateTransactionWithDefaults(t, blk_n);
    return toJS(call(blk_n, t, true).GasUsed);
  }

  string eth_getTransactionCount(string const& _address, string const& _blockNumber) override {
    return toJS(transaction_count(parse_blk_num(_blockNumber), toAddress(_address)));
  }

  Json::Value eth_getBlockTransactionCountByHash(string const& _blockHash) override {
    return toJson(transactionCount(jsToFixed<32>(_blockHash)));
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
                    t.to ? optional(t.to) : nullopt, chain_id);
    trx.rlp(true);
    return toJS(send_trx(trx));
  }

  string eth_sendRawTransaction(string const& _rlp) override {
    return toJS(send_trx(Transaction(jsToBytes(_rlp, OnFailed::Throw), true)));
  }

  Json::Value eth_getBlockByHash(string const& _blockHash, bool _includeTransactions) override {
    if (auto blk_n = final_chain->block_number(jsToFixed<32>(_blockHash)); blk_n) {
      return toJson(get_block_by_number(*blk_n, _includeTransactions));
    }
    return Json::Value();
  }

  Json::Value eth_getBlockByNumber(string const& _blockNumber, bool _includeTransactions) override {
    return toJson(get_block_by_number(parse_blk_num(_blockNumber), _includeTransactions));
  }

  Json::Value eth_getTransactionByHash(string const& _transactionHash) override {
    return toJson(get_transaction(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getTransactionByBlockHashAndIndex(string const& _blockHash,
                                                    string const& _transactionIndex) override {
    return toJson(get_transaction(jsToFixed<32>(_blockHash), jsToInt(_transactionIndex)));
  }

  Json::Value eth_getTransactionByBlockNumberAndIndex(string const& _blockNumber,
                                                      string const& _transactionIndex) override {
    return toJson(get_transaction(jsToInt(_transactionIndex), parse_blk_num(_blockNumber)));
  }

  Json::Value eth_getTransactionReceipt(string const& _transactionHash) override {
    return toJson(get_transaction_receipt(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getUncleByBlockHashAndIndex(string const&, string const&) override { return Json::Value(); }

  Json::Value eth_getUncleByBlockNumberAndIndex(string const&, string const&) override { return Json::Value(); }

  string eth_newFilter(Json::Value const& _json) override { return toJS(filters.newLogFilter(toLogFilter(_json))); }

  string eth_newBlockFilter() override { return toJS(filters.newBlockFilter()); }

  string eth_newPendingTransactionFilter() override { return toJS(filters.newPendingTransactionFilter()); }

  bool eth_uninstallFilter(string const& _filterId) override { return filters.uninstallFilter(jsToInt(_filterId)); }

  Json::Value eth_getFilterChanges(string const& _filterId) override { return filters.poll(jsToInt(_filterId)); }

  Json::Value eth_getFilterLogs(string const& _filterId) override {
    if (auto filter = filters.getLogFilter(jsToInt(_filterId))) {
      return toJson(logs(*filter));
    }
    return Json::Value(Json::arrayValue);
  }

  Json::Value eth_getLogs(Json::Value const& _json) override { return toJson(logs(toLogFilter(_json))); }

  Json::Value eth_syncing() override {
    // TODO
    return Json::Value(false);
  }

  Json::Value eth_chainId() override { return chain_id ? Json::Value(toJS(chain_id)) : Json::Value(); }

  void note_block(h256 const& blk_hash) override { filters.note_block(blk_hash); }

  void note_pending_transactions(RangeView<h256> const& trx_hashes) override {
    filters.note_pending_transactions(trx_hashes);
  }

  void note_receipts(RangeView<TransactionReceipt> const& receipts) override { filters.note_receipts(receipts); }

  Json::Value get_block_by_number(BlockNumber blk_n, bool include_transactions) {
    auto blk_header = final_chain->block_header(blk_n);
    if (!blk_header) {
      return Json::Value();
    }
    auto ret = toJson(*blk_header);
    auto& trxs_json = ret["transactions"] = Json::Value(Json::arrayValue);
    if (include_transactions) {
      ExtendedTransactionLocation loc;
      loc.blk_n = blk_header->number;
      loc.blk_h = blk_header->hash;
      for (auto const& t : final_chain->transactions(blk_n)) {
        trxs_json.append(toJson(t, loc));
        ++loc.index;
      }
    } else {
      final_chain->transaction_hashes(blk_n)->for_each([&](auto const& trx_hash) {
        trxs_json.append(toJson(trx_hash));
        //
      });
    }
    return ret;
  }

  optional<LocalisedTransaction> get_transaction(h256 const& h) const {
    auto trx = get_trx(h);
    if (!trx) {
      return {};
    }
    auto loc = final_chain->transaction_location(h);
    return LocalisedTransaction{move(*trx),
                                TransactionLocationWithBlockHash{*loc, *final_chain->block_hash(loc->blk_n)}};
  }

  optional<LocalisedTransaction> get_transaction(uint64_t i, BlockNumber n) const {
    auto hashes = final_chain->transaction_hashes(n);
    if (hashes->count() <= i) {
      return {};
    }
    auto trx = get_trx(hashes->get(i));
    return LocalisedTransaction{move(*trx), TransactionLocationWithBlockHash{n, i, *final_chain->block_hash(n)}};
  }

  optional<LocalisedTransaction> get_transaction(h256 const& _blockHash, uint64_t _i) const {
    auto blk_n = final_chain->block_number(_blockHash);
    if (!blk_n) {
      return {};
    }
    return get_transaction(_i, *blk_n);
  }

  optional<LocalisedTransactionReceipt> get_transaction_receipt(h256 const& trx_h) const {
    auto r = final_chain->transaction_receipt(trx_h);
    if (!r) {
      return {};
    }
    auto loc_trx = get_transaction(trx_h);
    auto const& trx = loc_trx->trx;
    return LocalisedTransactionReceipt{*r, ExtendedTransactionLocation{*loc_trx->trx_loc, trx_h}, trx.getSender(),
                                       trx.getReceiver()};
  }

  uint64_t transactionCount(h256 const& block_hash) const {
    auto n = final_chain->block_number(block_hash);
    return n ? final_chain->transactionCount(n) : 0;
  }

  uint64_t transaction_count(BlockNumber n, Address const& addr) {
    return final_chain->get_account(addr, n).value_or(ZeroAccount).Nonce;
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

  bool is_range_only(LogFilter const& f) const {
    if (!f.addresses.empty()) {
      return false;
    }
    for (auto const& t : f.topics) {
      if (!t.empty()) {
        return false;
      }
    }
    return true;
  }

  /// @returns bloom possibilities for all addresses and topics
  std::vector<LogBloom> bloomPossibilities(LogFilter const& f) const {
    // return combination of each of the addresses/topics
    vector<LogBloom> ret;
    // | every address with every topic
    for (auto const& i : f.addresses) {
      // 1st case, there are addresses and topics
      //
      // m_addresses = [a0, a1];
      // m_topics = [[t0], [t1a, t1b], [], []];
      //
      // blooms = [
      // a0 | t0, a0 | t1a | t1b,
      // a1 | t0, a1 | t1a | t1b
      // ]
      //
      for (auto const& t : f.topics) {
        if (t.empty()) {
          continue;
        }
        auto b = LogBloom().shiftBloom<3>(sha3(i));
        for (auto const& j : t) {
          b = b.shiftBloom<3>(sha3(j));
        }
        ret.push_back(b);
      }
    }

    // 2nd case, there are no topics
    //
    // m_addresses = [a0, a1];
    // m_topics = [[t0], [t1a, t1b], [], []];
    //
    // blooms = [a0, a1];
    //
    if (ret.empty()) {
      for (auto const& i : f.addresses) {
        ret.push_back(LogBloom().shiftBloom<3>(sha3(i)));
      }
    }

    // 3rd case, there are no addresses, at least create blooms from topics
    //
    // m_addresses = [];
    // m_topics = [[t0], [t1a, t1b], [], []];
    //
    // blooms = [t0, t1a | t1b];
    //
    if (f.addresses.empty()) {
      for (auto const& t : f.topics) {
        if (t.size()) {
          LogBloom b;
          for (auto const& j : t) {
            b = b.shiftBloom<3>(sha3(j));
          }
          ret.push_back(b);
        }
      }
    }
    return ret;
  }

  // TODO bloom const&
  bool matches(LogFilter const& f, LogBloom b) const {
    if (!f.addresses.empty()) {
      auto ok = false;
      for (auto const& i : f.addresses) {
        if (b.containsBloom<3>(sha3(i))) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return false;
      }
    }
    for (auto const& t : f.topics) {
      if (t.empty()) {
        continue;
      }
      auto ok = false;
      for (auto const& i : t) {
        if (b.containsBloom<3>(sha3(i))) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return false;
      }
    }
    return true;
  }

  vector<size_t> matches(LogFilter const& f, TransactionReceipt const& r) const {
    vector<size_t> ret;
    if (!matches(f, r.bloom())) {
      return ret;
    }
    for (uint log_i = 0; log_i < r.logs.size(); ++log_i) {
      auto const& e = r.logs[log_i];
      if (!f.addresses.empty() && !f.addresses.count(e.address)) {
        continue;
      }
      auto ok = true;
      for (size_t i = 0; i < f.topics.size(); ++i) {
        if (!f.topics[i].empty() && (e.topics.size() < i || !f.topics[i].count(e.topics[i]))) {
          ok = false;
          break;
        }
      }
      if (ok) {
        ret.push_back(log_i);
      }
    }
    return ret;
  }

  vector<LocalisedLogEntry> logs(LogFilter const& f) const {
    auto range_only = is_range_only(f);
    vector<LocalisedLogEntry> ret;
    auto action = [&](BlockNumber blk_n) {
      TransactionLocationWithBlockHash trx_loc{blk_n, 0, *final_chain->block_hash(blk_n)};
      final_chain->transaction_hashes(trx_loc.blk_n)->for_each([&](auto const& trx_h) {
        auto r = *final_chain->transaction_receipt(trx_h);
        auto action = [&](size_t log_i) {
          ret.push_back({r.logs[log_i], {trx_loc, trx_h}, log_i});
          //
        };
        if (range_only) {
          for (size_t i = 0; i < r.logs.size(); ++i) {
            action(i);
          }
        } else {
          for (auto i : matches(f, r)) {
            action(i);
          }
        }
        ++trx_loc.index;
      });
    };
    if (range_only) {
      for (auto blk_n = f.from_block; blk_n <= f.to_block; ++blk_n) {
        action(blk_n);
      }
      return ret;
    }
    set<BlockNumber> matchingBlocks;
    for (auto const& bloom : bloomPossibilities(f)) {
      for (auto blk_n : final_chain->withBlockBloom(bloom, f.from_block, f.to_block)) {
        matchingBlocks.insert(blk_n);
      }
    }
    for (auto blk_n : matchingBlocks) {
      action(blk_n);
    }
    return ret;
  }

  DEV_SIMPLE_EXCEPTION(InvalidAddress);
  static Address toAddress(string const& s) {
    try {
      if (auto b = fromHex(s.substr(0, 2) == "0x" ? s.substr(2) : s, WhenError::Throw); b.size() == Address::size) {
        return Address(b);
      }
    } catch (BadHexCharacter&) {
    }
    BOOST_THROW_EXCEPTION(InvalidAddress());
  }

  static TransactionSkeleton toTransactionSkeleton(Json::Value const& _json) {
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

  BlockNumber parse_blk_num(string const& json_str, optional<BlockNumber> latest_block = {}) {
    if (json_str == "latest" || json_str == "pending") {
      return latest_block ? *latest_block : final_chain->last_block_number();
    }
    return json_str == "earliest" ? 0 : jsToInt(json_str);
  }

  LogFilter toLogFilter(Json::Value const& json) {
    auto latest_block = final_chain->last_block_number();
    LogFilter filter{latest_block, latest_block};
    if (!json.isObject() || json.empty()) {
      return filter;
    }
    if (auto const& fromBlock = json["fromBlock"]; !fromBlock.empty()) {
      filter.from_block = parse_blk_num(fromBlock.asString(), latest_block);
    }
    if (auto const& toBlock = json["toBlock"]; !toBlock.empty()) {
      filter.to_block = parse_blk_num(toBlock.asString(), latest_block);
    }
    if (auto const& address = json["address"]; !address.empty()) {
      if (address.isArray()) {
        for (auto const& i : address) {
          filter.addresses.insert(toAddress(i.asString()));
        }
      } else {
        filter.addresses.insert(toAddress(address.asString()));
      }
    }
    if (auto const& topics = json["topics"]; !topics.empty()) {
      for (uint32_t i = 0; i < topics.size(); i++) {
        auto const& topic = topics[i];
        if (topic.isArray()) {
          for (auto const& t : topic) {
            if (!t.isNull()) {
              filter.topics[i].insert(jsToFixed<32>(t.asString()));
            }
          }
        } else if (!topic.isNull()) {
          filter.topics[i].insert(jsToFixed<32>(topic.asString()));
        }
      }
    }
    return filter;
  }

  static void add(Json::Value& obj, optional<TransactionLocationWithBlockHash> const& info) {
    obj["blockNumber"] = info ? toJson(info->blk_n) : Json::Value();
    obj["blockHash"] = info ? toJson(info->blk_h) : Json::Value();
    obj["transactionIndex"] = info ? toJson(info->index) : Json::Value();
  }

  static void add(Json::Value& obj, ExtendedTransactionLocation const& info) {
    add(obj, TransactionLocationWithBlockHash(info));
    obj["transactionHash"] = toJson(info.trx_hash);
  }

  static Json::Value toJson(Transaction const& trx, optional<TransactionLocationWithBlockHash> const& loc) {
    Json::Value res(Json::objectValue);
    add(res, loc);
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

  static Json::Value toJson(LocalisedTransaction const& lt) { return toJson(lt.trx, lt.trx_loc); }

  static Json::Value toJson(BlockHeader const& obj) {
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

  static Json::Value toJson(LocalisedLogEntry const& lle) {
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

  static Json::Value toJson(LocalisedTransactionReceipt const& ltr) {
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

  template <typename T>
  static Json::Value toJson(T const& t) {
    return toJS(t);
  }

  template <typename T>
  static Json::Value toJson(vector<T> const& _es) {
    Json::Value res(Json::arrayValue);
    for (auto const& e : _es) {
      res.append(toJson(e));
    }
    return res;
  }

  template <typename T>
  static Json::Value toJson(optional<T> const& t) {
    return t ? toJson(*t) : Json::Value();
  }
};

Json::Value toJson(BlockHeader const& obj) { return EthImpl::toJson(obj); }

Eth* NewEth(EthParams&& prerequisites) { return new EthImpl(move(prerequisites)); }

}  // namespace taraxa::net::rpc::eth