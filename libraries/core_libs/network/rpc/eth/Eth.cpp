#include "Eth.h"

#include <json/json.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include <stdexcept>

#include "LogFilter.hpp"
#include "common/rpc_utils.hpp"
#include "common/types.hpp"
using namespace std;
using namespace dev;
using namespace taraxa::final_chain;
using namespace taraxa::state_api;

namespace taraxa::net::rpc::eth {
void add(Json::Value& obj, const optional<TransactionLocationWithBlockHash>& info) {
  obj["blockNumber"] = info ? toJS(info->period) : Json::Value();
  obj["blockHash"] = info ? toJS(info->blk_h) : Json::Value();
  obj["transactionIndex"] = info ? toJS(info->position) : Json::Value();
}

void add(Json::Value& obj, const ExtendedTransactionLocation& info) {
  add(obj, static_cast<const TransactionLocationWithBlockHash&>(info));
  obj["transactionHash"] = toJS(info.trx_hash);
}

Json::Value toJson(const Transaction& trx, const optional<TransactionLocationWithBlockHash>& loc) {
  Json::Value res = trx.toJSON();
  add(res, loc);
  return res;
}

Json::Value toJson(const LocalisedTransaction& lt) { return toJson(*lt.trx, lt.trx_loc); }

Json::Value toJson(const BlockHeader& obj) {
  Json::Value res(Json::objectValue);
  res["parentHash"] = toJS(obj.parent_hash);
  res["sha3Uncles"] = toJS(BlockHeader::unclesHash());
  res["stateRoot"] = toJS(obj.state_root);
  res["transactionsRoot"] = toJS(obj.transactions_root);
  res["receiptsRoot"] = toJS(obj.receipts_root);
  res["number"] = toJS(obj.number);
  res["gasUsed"] = toJS(obj.gas_used);
  res["gasLimit"] = toJS(obj.gas_limit);
  res["extraData"] = toJS(obj.extra_data);
  res["logsBloom"] = toJS(obj.log_bloom);
  res["timestamp"] = toJS(obj.timestamp);
  res["author"] = toJS(obj.author);
  res["miner"] = toJS(obj.author);
  res["mixHash"] = toJS(BlockHeader::mixHash());
  res["nonce"] = toJS(BlockHeader::nonce());
  res["uncles"] = Json::Value(Json::arrayValue);
  res["hash"] = toJS(obj.hash);
  res["difficulty"] = "0x0";
  res["totalDifficulty"] = "0x0";
  res["totalReward"] = toJS(obj.total_reward);
  res["size"] = toJS(obj.size);
  return res;
}

Json::Value toJson(const LocalisedLogEntry& lle) {
  Json::Value res(Json::objectValue);
  add(res, lle.trx_loc);
  res["removed"] = false;
  res["data"] = toJS(lle.le.data);
  res["address"] = toJS(lle.le.address);
  res["logIndex"] = toJS(lle.position_in_receipt);
  auto& topics_json = res["topics"] = Json::Value(Json::arrayValue);
  for (const auto& t : lle.le.topics) {
    topics_json.append(toJS(t));
  }
  return res;
}

Json::Value toJson(const LocalisedTransactionReceipt& ltr) {
  Json::Value res(Json::objectValue);
  add(res, ltr.trx_loc);
  res["from"] = toJS(ltr.trx_from);
  res["to"] = toJson(ltr.trx_to);
  res["status"] = toJS(ltr.r.status_code);
  res["gasUsed"] = toJS(ltr.r.gas_used);
  res["cumulativeGasUsed"] = toJS(ltr.r.cumulative_gas_used);
  res["contractAddress"] = toJson(ltr.r.new_contract_address);
  res["logsBloom"] = toJS(ltr.r.bloom());

  auto& logs_json = res["logs"] = Json::Value(Json::arrayValue);
  uint log_i = 0;
  for (const auto& le : ltr.r.logs) {
    logs_json.append(toJson(LocalisedLogEntry{le, ltr.trx_loc, log_i++}));
  }
  return res;
}

Json::Value toJson(const SyncStatus& obj) {
  Json::Value res(Json::objectValue);
  res["startingBlock"] = toJS(obj.starting_block);
  res["currentBlock"] = toJS(obj.current_block);
  res["highestBlock"] = toJS(obj.highest_block);
  return res;
}

DEV_SIMPLE_EXCEPTION(InvalidAddress);
Address toAddress(const std::string& s) {
  try {
    if (auto b = fromHex(s.substr(0, 2) == "0x" ? s.substr(2) : s, dev::WhenError::Throw); b.size() == Address::size) {
      return Address(b);
    }
  } catch (dev::BadHexCharacter&) {
  }
  BOOST_THROW_EXCEPTION(InvalidAddress());
}

class EthImpl : public Eth, EthParams {
  Watches watches_;

 public:
  EthImpl(EthParams&& prerequisites) : EthParams(std::move(prerequisites)), watches_(watches_cfg) {}

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"eth", "1.0"}}; }

  string eth_protocolVersion() override { return toJS(63); }

  string eth_coinbase() override { return toJS(address); }

  string eth_gasPrice() override { return toJS(gas_pricer()); }

  Json::Value eth_accounts() override { return toJsonArray(vector{address}); }

  string eth_blockNumber() override { return toJS(final_chain->lastBlockNumber()); }

  string eth_getBalance(const string& _address, const Json::Value& _json) override {
    const auto block_number = get_block_number_from_json(_json);
    return toJS(final_chain->getAccount(toAddress(_address), block_number).value_or(ZeroAccount).balance);
  }

  string eth_getStorageAt(const string& _address, const string& _position, const Json::Value& _json) override {
    const auto block_number = get_block_number_from_json(_json);
    return toJS(final_chain->getAccountStorage(toAddress(_address), jsToU256(_position), block_number));
  }

  string eth_getStorageRoot(const string& _address, const string& _blockNumber) override {
    return toJS(final_chain->getAccount(toAddress(_address), parse_blk_num(_blockNumber))
                    .value_or(ZeroAccount)
                    .storage_root_eth());
  }

  string eth_getCode(const string& _address, const Json::Value& _json) override {
    const auto block_number = get_block_number_from_json(_json);
    return toJS(final_chain->getCode(toAddress(_address), block_number));
  }

  string eth_call(const Json::Value& _json, const Json::Value& _jsonBlock) override {
    const auto block_number = get_block_number_from_json(_jsonBlock);
    auto t = toTransactionSkeleton(_json);
    prepare_transaction_for_call(t, block_number);
    auto ret = call(block_number, t);
    if (!ret.consensus_err.empty() || !ret.code_err.empty()) {
      if (ret.code_retval.empty()) {
        throw jsonrpc::JsonRpcException(ret.consensus_err.empty() ? ret.code_err : ret.consensus_err);
      }
      throw jsonrpc::JsonRpcException(CALL_EXCEPTION, ret.consensus_err.empty() ? ret.code_err : ret.consensus_err,
                                      toJS(ret.code_retval));
    }
    return toJS(ret.code_retval);
  }

  string eth_estimateGas(const Json::Value& _json, const string& blockNumber) override {
    auto t = toTransactionSkeleton(_json);
    EthBlockNumber blk_n;
    if (!blockNumber.empty()) {
      blk_n = parse_blk_num(blockNumber);
    } else {
      blk_n = final_chain->lastBlockNumber();
    }
    prepare_transaction_for_call(t, blk_n);

    auto is_enough_gas = [&](gas_t gas) -> bool {
      t.gas = gas;
      auto res = call(blk_n, t);
      if (!res.consensus_err.empty()) {
        throw std::runtime_error(res.consensus_err);
      }
      if (!res.code_err.empty()) {
        return false;
      }
      return true;
    };
    // couldn't be lower than execution gas_used. So we should start with this value
    auto call_result = call(blk_n, t);
    if (!call_result.consensus_err.empty() || !call_result.code_err.empty()) {
      throw std::runtime_error(call_result.consensus_err.empty() ? call_result.code_err : call_result.consensus_err);
    }
    gas_t low = call_result.gas_used;
    gas_t hi = *t.gas;
    if (low > hi) {
      throw std::runtime_error("out of gas");
    }
    // precision is 5%(1/20) of higher gas_used value
    while (hi - low > hi / 20) {
      auto mid = low + ((hi - low) / 2);

      if (is_enough_gas(mid)) {
        hi = mid;
      } else {
        low = mid;
      }
    }
    return toJS(hi);
  }

  string eth_getTransactionCount(const string& _address, const Json::Value& _json) override {
    const auto block_number = get_block_number_from_json(_json);
    return toJS(transaction_count(block_number, toAddress(_address)));
  }

  Json::Value eth_getBlockTransactionCountByHash(const string& _blockHash) override {
    return toJS(transactionCount(jsToFixed<32>(_blockHash)));
  }

  Json::Value eth_getBlockTransactionCountByNumber(const string& _blockNumber) override {
    return toJS(final_chain->transactionCount(parse_blk_num(_blockNumber)));
  }

  Json::Value eth_getUncleCountByBlockHash(const string&) override { return toJS(0); }

  Json::Value eth_getUncleCountByBlockNumber(const string&) override { return toJS(0); }

  string eth_sendRawTransaction(const string& _rlp) override {
    auto trx = std::make_shared<Transaction>(jsToBytes(_rlp, OnFailed::Throw), true);
    send_trx(trx);
    return toJS(trx->getHash());
  }

  Json::Value eth_getBlockByHash(const string& _blockHash, bool _includeTransactions) override {
    if (auto blk_n = final_chain->blockNumber(jsToFixed<32>(_blockHash)); blk_n) {
      return get_block_by_number(*blk_n, _includeTransactions);
    }
    return Json::Value();
  }

  Json::Value eth_getBlockByNumber(const string& _blockNumber, bool _includeTransactions) override {
    return get_block_by_number(parse_blk_num(_blockNumber), _includeTransactions);
  }

  Json::Value eth_getTransactionByHash(const string& _transactionHash) override {
    return toJson(get_transaction(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getTransactionByBlockHashAndIndex(const string& _blockHash,
                                                    const string& _transactionIndex) override {
    return toJson(get_transaction(jsToFixed<32>(_blockHash), jsToInt(_transactionIndex)));
  }

  Json::Value eth_getTransactionByBlockNumberAndIndex(const string& _blockNumber,
                                                      const string& _transactionIndex) override {
    return toJson(get_transaction(parse_blk_num(_blockNumber), jsToInt(_transactionIndex)));
  }

  Json::Value eth_getTransactionReceipt(const string& _transactionHash) override {
    return toJson(get_transaction_receipt(jsToFixed<32>(_transactionHash)));
  }

  Json::Value eth_getBlockReceipts(const Json::Value& _blockNumber) override {
    auto blk_n = get_block_number_from_json(_blockNumber);
    auto block_hash = final_chain->blockHash(blk_n);
    if (!block_hash) {
      return Json::Value(Json::arrayValue);
    }
    auto transactions = final_chain->transactions(blk_n);
    if (transactions.empty()) {
      return Json::Value(Json::arrayValue);
    }

    auto receipts = final_chain->blockReceipts(blk_n);
    return util::transformToJsonParallel(
        transactions, [this, &receipts, blk_n, &block_hash](const auto& trx, auto index) {
          if (!receipts) {
            return toJson(LocalisedTransactionReceipt{
                final_chain->transactionReceipt(blk_n, index, trx->getHash()).value(),
                ExtendedTransactionLocation{{{blk_n, index}, *block_hash}, trx->getHash()},
                trx->getSender(),
                trx->getReceiver(),
            });
          }
          return toJson(LocalisedTransactionReceipt{
              receipts->at(index),
              ExtendedTransactionLocation{{{blk_n, index}, *block_hash}, trx->getHash()},
              trx->getSender(),
              trx->getReceiver(),
          });
        });
  }

  Json::Value eth_getUncleByBlockHashAndIndex(const string&, const string&) override { return Json::Value(); }

  Json::Value eth_getUncleByBlockNumberAndIndex(const string&, const string&) override { return Json::Value(); }

  string eth_newFilter(const Json::Value& _json) override {
    return toJS(watches_.logs_.install_watch(parse_log_filter(_json)));
  }

  string eth_newBlockFilter() override { return toJS(watches_.new_blocks_.install_watch()); }

  string eth_newPendingTransactionFilter() override { return toJS(watches_.new_transactions_.install_watch()); }

  bool eth_uninstallFilter(const string& _filterId) override {
    auto watch_id = jsToInt(_filterId);
    return watches_.visit_by_id(watch_id, [=](auto watch) { return watch && watch->uninstall_watch(watch_id); });
  }

  Json::Value eth_getFilterChanges(const string& _filterId) override {
    auto watch_id = jsToInt(_filterId);
    return watches_.visit_by_id(watch_id, [=](auto watch) {
      return watch ? toJsonArray(watch->poll(watch_id)) : Json::Value(Json::arrayValue);
    });
  }

  Json::Value eth_getFilterLogs(const string& _filterId) override {
    if (auto filter = watches_.logs_.get_watch_params(jsToInt(_filterId))) {
      return toJsonArray(filter->match_all(*final_chain));
    }
    return Json::Value(Json::arrayValue);
  }

  Json::Value eth_getLogs(const Json::Value& _json) override {
    return toJsonArray(parse_log_filter(_json).match_all(*final_chain));
  }

  Json::Value eth_syncing() override {
    auto status = syncing_probe();
    return status ? toJson(status) : Json::Value(false);
  }

  Json::Value eth_chainId() override { return chain_id ? Json::Value(toJS(chain_id)) : Json::Value(); }

  void note_block_executed(const BlockHeader& blk_header, const SharedTransactions& trxs,
                           const TransactionReceipts& receipts) override {
    watches_.new_blocks_.process_update(blk_header.hash);
    ExtendedTransactionLocation trx_loc{{{blk_header.number}, blk_header.hash}};
    for (; trx_loc.position < trxs.size(); ++trx_loc.position) {
      trx_loc.trx_hash = trxs[trx_loc.position]->getHash();
      using LogsInput = typename decltype(watches_.logs_)::InputType;
      watches_.logs_.process_update(LogsInput(trx_loc, receipts[trx_loc.position]));
    }
  }

  void note_pending_transaction(const h256& trx_hash) override { watches_.new_transactions_.process_update(trx_hash); }

  Json::Value get_block_by_number(EthBlockNumber blk_n, bool include_transactions) {
    auto blk_header = final_chain->blockHeader(blk_n);
    if (!blk_header) {
      return Json::Value();
    }
    auto ret = toJson(*blk_header);
    auto& trxs_json = ret["transactions"] = Json::Value(Json::arrayValue);
    if (include_transactions) {
      ExtendedTransactionLocation loc;
      loc.period = blk_header->number;
      loc.blk_h = blk_header->hash;
      for (const auto& t : final_chain->transactions(blk_n)) {
        trxs_json.append(toJson(*t, loc));
        ++loc.position;
      }
    } else {
      auto hashes = final_chain->transactionHashes(blk_n);
      trxs_json = toJsonArray(*hashes);
    }
    return ret;
  }

  optional<LocalisedTransaction> get_transaction(const h256& h) const {
    auto trx = get_trx(h);
    if (!trx) {
      return {};
    }
    auto loc = final_chain->transactionLocation(h);
    return LocalisedTransaction{
        trx,
        TransactionLocationWithBlockHash{
            *loc,
            *final_chain->blockHash(loc->period),
        },
    };
  }

  optional<LocalisedTransaction> get_transaction(EthBlockNumber blk_n, uint32_t trx_pos) const {
    const auto& trxs = final_chain->transactions(blk_n);
    if (trxs.size() <= trx_pos) {
      return {};
    }
    return LocalisedTransaction{
        trxs[trx_pos],
        TransactionLocationWithBlockHash{
            {blk_n, trx_pos},
            *final_chain->blockHash(blk_n),
        },
    };
  }

  optional<LocalisedTransaction> get_transaction(const h256& blk_h, uint64_t _i) const {
    auto blk_n = final_chain->blockNumber(blk_h);
    if (!blk_n) {
      return {};
    }
    return get_transaction(*blk_n, _i);
  }

  optional<LocalisedTransactionReceipt> get_transaction_receipt(const h256& trx_h) const {
    auto location = final_chain->transactionLocation(trx_h);
    if (!location) {
      return {};
    }
    auto r = final_chain->transactionReceipt(location->period, location->position, trx_h);
    if (!r) {
      return {};
    }
    auto loc_trx = get_transaction(location->period, location->position);
    const auto& trx = loc_trx->trx;
    return LocalisedTransactionReceipt{
        *r,
        ExtendedTransactionLocation{*loc_trx->trx_loc, trx_h},
        trx->getSender(),
        trx->getReceiver(),
    };
  }

  uint64_t transactionCount(const h256& block_hash) const {
    auto n = final_chain->blockNumber(block_hash);
    return n ? final_chain->transactionCount(n) : 0;
  }

  trx_nonce_t transaction_count(EthBlockNumber n, const Address& addr) {
    return final_chain->getAccount(addr, n).value_or(ZeroAccount).nonce;
  }

  state_api::ExecutionResult call(EthBlockNumber blk_n, const TransactionSkeleton& trx) {
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
        blk_n);

    return result;
  }

  // this should be used only in eth_call and eth_estimateGas
  void prepare_transaction_for_call(TransactionSkeleton& t, EthBlockNumber blk_n) {
    if (!t.from) {
      t.from = ZeroAddress;
    }
    if (!t.nonce) {
      t.nonce = transaction_count(blk_n, t.from);
    }
    if (!t.gas_price) {
      t.gas_price = 0;
    }
    if (!t.gas) {
      t.gas = gas_limit;
    }
  }

  static TransactionSkeleton toTransactionSkeleton(const Json::Value& _json) {
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
    } else if (!_json["input"].empty()) {
      ret.data = jsToBytes(_json["input"].asString(), OnFailed::Throw);
    }
    if (!_json["code"].empty()) {
      ret.data = jsToBytes(_json["code"].asString(), OnFailed::Throw);
    }
    if (!_json["nonce"].empty()) {
      ret.nonce = jsToU256(_json["nonce"].asString());
    }
    return ret;
  }

  static optional<EthBlockNumber> parse_blk_num_specific(const string& blk_num_str) {
    if (blk_num_str == "latest" || blk_num_str == "pending" || blk_num_str == "safe" || blk_num_str == "finalized") {
      return std::nullopt;
    }
    return blk_num_str == "earliest" ? 0 : jsToInt(blk_num_str);
  }

  EthBlockNumber parse_blk_num(const string& blk_num_str) {
    auto ret = parse_blk_num_specific(blk_num_str);
    return ret ? *ret : final_chain->lastBlockNumber();
  }

  EthBlockNumber get_block_number_from_json(const Json::Value& json) {
    if (json.isObject()) {
      if (!json["blockNumber"].empty()) {
        return parse_blk_num(json["blockNumber"].asString());
      }
      if (!json["blockHash"].empty()) {
        if (auto ret = final_chain->blockNumber(jsToFixed<32>(json["blockHash"].asString()))) {
          return *ret;
        }
        throw std::runtime_error("Resource not found");
      }
    }
    return parse_blk_num(json.asString());
  }

  LogFilter parse_log_filter(const Json::Value& json) {
    EthBlockNumber from_block;
    optional<EthBlockNumber> to_block;
    if (const auto& fromBlock = json["fromBlock"]; !fromBlock.empty()) {
      from_block = parse_blk_num(fromBlock.asString());
    } else {
      from_block = final_chain->lastBlockNumber();
    }
    if (const auto& toBlock = json["toBlock"]; !toBlock.empty()) {
      to_block = parse_blk_num_specific(toBlock.asString());
    }
    return LogFilter(from_block, to_block, parse_addresses(json), parse_topics(json));
  }
};

shared_ptr<Eth> NewEth(EthParams&& prerequisites) { return make_shared<EthImpl>(std::move(prerequisites)); }

}  // namespace taraxa::net::rpc::eth