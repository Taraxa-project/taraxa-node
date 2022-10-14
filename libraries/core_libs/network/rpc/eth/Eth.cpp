#include "Eth.h"

#include <json/json.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include <stdexcept>

#include "LogFilter.hpp"

namespace taraxa::net::rpc::eth {
using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::final_chain;
using namespace ::taraxa::state_api;

class EthImpl : public Eth, EthParams {
  Watches watches_;

 public:
  EthImpl(EthParams&& prerequisites) : EthParams(move(prerequisites)), watches_(watches_cfg) {}

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"eth", "1.0"}}; }

  string eth_protocolVersion() override { return toJS(63); }

  string eth_coinbase() override { return toJS(address); }

  string eth_gasPrice() override { return toJS(gas_pricer()); }

  Json::Value eth_accounts() override { return toJsonArray(vector{address}); }

  string eth_blockNumber() override { return toJS(final_chain->last_block_number()); }

  string eth_getBalance(string const& _address, string const& _blockNumber) override {
    return toJS(
        final_chain->get_account(toAddress(_address), parse_blk_num(_blockNumber)).value_or(ZeroAccount).balance);
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
    set_transaction_defaults(t, blk_n);
    return toJS(call(blk_n, t).code_retval);
  }

  string eth_estimateGas(Json::Value const& _json) override {
    auto t = toTransactionSkeleton(_json);
    if (!t.gas) {
      t.gas = FinalChain::GAS_LIMIT;
    }
    auto blk_n = final_chain->last_block_number();
    set_transaction_defaults(t, blk_n);
    return toJS(call(blk_n, t).gas_used);
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

  string eth_sendRawTransaction(string const& _rlp) override {
    auto trx = std::make_shared<Transaction>(jsToBytes(_rlp, OnFailed::Throw), true);
    send_trx(trx);
    return toJS(trx->getHash());
  }

  Json::Value eth_getBlockByHash(string const& _blockHash, bool _includeTransactions) override {
    if (auto blk_n = final_chain->block_number(jsToFixed<32>(_blockHash)); blk_n) {
      return get_block_by_number(*blk_n, _includeTransactions);
    }
    return Json::Value();
  }

  Json::Value eth_getBlockByNumber(string const& _blockNumber, bool _includeTransactions) override {
    return get_block_by_number(parse_blk_num(_blockNumber), _includeTransactions);
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

  string eth_newFilter(Json::Value const& _json) override {
    return toJS(watches_.logs_.install_watch(parse_log_filter(_json)));
  }

  string eth_newBlockFilter() override { return toJS(watches_.new_blocks_.install_watch()); }

  string eth_newPendingTransactionFilter() override { return toJS(watches_.new_transactions_.install_watch()); }

  bool eth_uninstallFilter(string const& _filterId) override {
    auto watch_id = jsToInt(_filterId);
    return watches_.visit_by_id(watch_id, [=](auto watch) { return watch && watch->uninstall_watch(watch_id); });
  }

  Json::Value eth_getFilterChanges(string const& _filterId) override {
    auto watch_id = jsToInt(_filterId);
    return watches_.visit_by_id(watch_id, [=](auto watch) {
      return watch ? toJsonArray(watch->poll(watch_id)) : Json::Value(Json::arrayValue);
    });
  }

  Json::Value eth_getFilterLogs(string const& _filterId) override {
    if (auto filter = watches_.logs_.get_watch_params(jsToInt(_filterId))) {
      return toJsonArray(filter->match_all(*final_chain));
    }
    return Json::Value(Json::arrayValue);
  }

  Json::Value eth_getLogs(Json::Value const& _json) override {
    return toJsonArray(parse_log_filter(_json).match_all(*final_chain));
  }

  Json::Value eth_syncing() override {
    auto status = syncing_probe();
    return status ? toJson(status) : Json::Value(false);
  }

  Json::Value eth_chainId() override { return chain_id ? Json::Value(toJS(chain_id)) : Json::Value(); }

  void note_block_executed(BlockHeader const& blk_header, SharedTransactions const& trxs,
                           TransactionReceipts const& receipts) override {
    watches_.new_blocks_.process_update(blk_header.hash);
    ExtendedTransactionLocation trx_loc{{{blk_header.number}, blk_header.hash}};
    for (; trx_loc.index < trxs.size(); ++trx_loc.index) {
      trx_loc.trx_hash = trxs[trx_loc.index]->getHash();
      using LogsInput = typename decltype(watches_.logs_)::InputType;
      watches_.logs_.process_update(LogsInput(trx_loc, receipts[trx_loc.index]));
    }
  }

  void note_pending_transaction(h256 const& trx_hash) override { watches_.new_transactions_.process_update(trx_hash); }

  Json::Value get_block_by_number(EthBlockNumber blk_n, bool include_transactions) {
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
        trxs_json.append(toJson(*t, loc));
        ++loc.index;
      }
    } else {
      auto hashes = final_chain->transaction_hashes(blk_n);
      for (size_t i = 0; i < hashes->count(); ++i) {
        trxs_json.append(toJson(hashes->get(i)));
      }
    }
    return ret;
  }

  optional<LocalisedTransaction> get_transaction(h256 const& h) const {
    auto trx = get_trx(h);
    if (!trx) {
      return {};
    }
    auto loc = final_chain->transaction_location(h);
    return LocalisedTransaction{
        trx,
        TransactionLocationWithBlockHash{
            *loc,
            *final_chain->block_hash(loc->blk_n),
        },
    };
  }

  optional<LocalisedTransaction> get_transaction(uint64_t trx_pos, EthBlockNumber blk_n) const {
    auto hashes = final_chain->transaction_hashes(blk_n);
    if (hashes->count() <= trx_pos) {
      return {};
    }
    return LocalisedTransaction{
        get_trx(hashes->get(trx_pos)),
        TransactionLocationWithBlockHash{
            {blk_n, trx_pos},
            *final_chain->block_hash(blk_n),
        },
    };
  }

  optional<LocalisedTransaction> get_transaction(h256 const& blk_h, uint64_t _i) const {
    auto blk_n = final_chain->block_number(blk_h);
    return blk_n ? get_transaction(_i, *blk_n) : nullopt;
  }

  optional<LocalisedTransactionReceipt> get_transaction_receipt(h256 const& trx_h) const {
    auto r = final_chain->transaction_receipt(trx_h);
    if (!r) {
      return {};
    }
    auto loc_trx = get_transaction(trx_h);
    auto const& trx = loc_trx->trx;
    return LocalisedTransactionReceipt{
        *r,
        ExtendedTransactionLocation{*loc_trx->trx_loc, trx_h},
        trx->getSender(),
        trx->getReceiver(),
    };
  }

  uint64_t transactionCount(h256 const& block_hash) const {
    auto n = final_chain->block_number(block_hash);
    return n ? final_chain->transactionCount(n) : 0;
  }

  trx_nonce_t transaction_count(EthBlockNumber n, Address const& addr) {
    return final_chain->get_account(addr, n).value_or(ZeroAccount).nonce;
  }

  state_api::ExecutionResult call(EthBlockNumber blk_n, TransactionSkeleton const& trx) {
    const auto result = final_chain->call(
        {
            trx.from,
            trx.gas_price.value_or(0),
            trx.to,
            trx.nonce.value_or(0),
            trx.value,
            trx.gas.value_or(0),
            trx.data,
        },
        blk_n, state_api::ExecutionOptions{true, false});

    if (result.consensus_err.empty() && result.code_err.empty()) {
      return result;
    }
    throw std::runtime_error(result.consensus_err.empty() ? result.code_err : result.consensus_err);
  }

  void set_transaction_defaults(TransactionSkeleton& t, EthBlockNumber blk_n) {
    if (!t.from) {
      t.from = ZeroAddress;
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
      ret.nonce = jsToU256(_json["nonce"].asString());
    }
    return ret;
  }

  static optional<EthBlockNumber> parse_blk_num_specific(string const& blk_num_str) {
    if (blk_num_str == "latest" || blk_num_str == "pending") {
      return std::nullopt;
    }
    return blk_num_str == "earliest" ? 0 : jsToInt(blk_num_str);
  }

  EthBlockNumber parse_blk_num(string const& blk_num_str) {
    auto ret = parse_blk_num_specific(blk_num_str);
    return ret ? *ret : final_chain->last_block_number();
  }

  LogFilter parse_log_filter(Json::Value const& json) {
    EthBlockNumber from_block;
    optional<EthBlockNumber> to_block;
    AddressSet addresses;
    LogFilter::Topics topics;
    if (auto const& fromBlock = json["fromBlock"]; !fromBlock.empty()) {
      from_block = parse_blk_num(fromBlock.asString());
    } else {
      from_block = final_chain->last_block_number();
    }
    if (auto const& toBlock = json["toBlock"]; !toBlock.empty()) {
      to_block = parse_blk_num_specific(toBlock.asString());
    }
    if (auto const& address = json["address"]; !address.empty()) {
      if (address.isArray()) {
        for (auto const& obj : address) {
          addresses.insert(toAddress(obj.asString()));
        }
      } else {
        addresses.insert(toAddress(address.asString()));
      }
    }
    if (auto const& topics_json = json["topics"]; !topics_json.empty()) {
      for (uint32_t i = 0; i < topics_json.size(); i++) {
        auto const& topic_json = topics_json[i];
        if (topic_json.isArray()) {
          for (auto const& t : topic_json) {
            if (!t.isNull()) {
              topics[i].insert(jsToFixed<32>(t.asString()));
            }
          }
        } else if (!topic_json.isNull()) {
          topics[i].insert(jsToFixed<32>(topic_json.asString()));
        }
      }
    }
    return LogFilter(from_block, to_block, move(addresses), move(topics));
  }

  static void add(Json::Value& obj, optional<TransactionLocationWithBlockHash> const& info) {
    obj["blockNumber"] = info ? toJson(info->blk_n) : Json::Value();
    obj["blockHash"] = info ? toJson(info->blk_h) : Json::Value();
    obj["transactionIndex"] = info ? toJson(info->index) : Json::Value();
  }

  static void add(Json::Value& obj, ExtendedTransactionLocation const& info) {
    add(obj, static_cast<TransactionLocationWithBlockHash const&>(info));
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

  static Json::Value toJson(const LocalisedTransaction& lt) { return toJson(*lt.trx, lt.trx_loc); }

  static Json::Value toJson(BlockHeader const& obj) {
    Json::Value res(Json::objectValue);
    res["parentHash"] = toJson(obj.parent_hash);
    res["sha3Uncles"] = toJson(BlockHeader::uncles_hash());
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
    res["difficulty"] = "0x0";
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
    res["status"] = toJson(ltr.r.status_code);
    res["gasUsed"] = toJson(ltr.r.gas_used);
    res["cumulativeGasUsed"] = toJson(ltr.r.cumulative_gas_used);
    res["contractAddress"] = toJson(ltr.r.new_contract_address);
    res["logsBloom"] = toJson(ltr.r.bloom());
    auto& logs_json = res["logs"] = Json::Value(Json::arrayValue);
    uint log_i = 0;
    for (auto const& le : ltr.r.logs) {
      logs_json.append(toJson(LocalisedLogEntry{le, ltr.trx_loc, log_i++}));
    }
    return res;
  }

  static Json::Value toJson(SyncStatus const& obj) {
    Json::Value res(Json::objectValue);
    res["startingBlock"] = toJS(obj.starting_block);
    res["currentBlock"] = toJS(obj.current_block);
    res["highestBlock"] = toJS(obj.highest_block);
    return res;
  }

  template <typename T>
  static Json::Value toJson(T const& t) {
    return toJS(t);
  }

  template <typename T>
  static Json::Value toJsonArray(vector<T> const& _es) {
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

shared_ptr<Eth> NewEth(EthParams&& prerequisites) { return make_shared<EthImpl>(move(prerequisites)); }

}  // namespace taraxa::net::rpc::eth