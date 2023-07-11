#include "Debug.h"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "common/jsoncpp.hpp"
#include "final_chain/state_api_data.hpp"
#include "network/rpc/eth/data.hpp"
#include "pbft/pbft_manager.hpp"

using namespace std;
using namespace dev;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

inline EthBlockNumber get_ctx_block_num(EthBlockNumber block_number) {
  return (block_number >= 1) ? block_number - 1 : 0;
}

Json::Value Debug::debug_traceTransaction(const std::string& transaction_hash) {
  Json::Value res;
  try {
    auto [trx, loc] = get_transaction_with_location(transaction_hash);
    if (!trx || !loc) {
      res["status"] = "Transaction not found";
      return res;
    }
    if (auto node = full_node_.lock()) {
      return util::readJsonFromString(
          node->getFinalChain()->trace({to_eth_trx(std::move(trx))}, get_ctx_block_num(loc->blk_n)));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Debug::debug_traceCall(const Json::Value& call_params, const std::string& blk_num) {
  Json::Value res;
  try {
    const auto block = parse_blk_num(blk_num);
    auto trx = to_eth_trx(call_params, block);
    if (auto node = full_node_.lock()) {
      return util::readJsonFromString(node->getFinalChain()->trace({std::move(trx)}, block));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Debug::trace_call(const Json::Value& call_params, const Json::Value& trace_params,
                              const std::string& blk_num) {
  Json::Value res;
  try {
    const auto block = parse_blk_num(blk_num);
    auto params = parse_tracking_parms(trace_params);
    if (auto node = full_node_.lock()) {
      return util::readJsonFromString(
          node->getFinalChain()->trace({to_eth_trx(call_params, block)}, block, std::move(params)));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Debug::trace_replayTransaction(const std::string& transaction_hash, const Json::Value& trace_params) {
  Json::Value res;
  try {
    auto params = parse_tracking_parms(trace_params);
    auto [trx, loc] = get_transaction_with_location(transaction_hash);
    if (!trx || !loc) {
      res["status"] = "Transaction not found";
      return res;
    }
    if (auto node = full_node_.lock()) {
      return util::readJsonFromString(
          node->getFinalChain()->trace({to_eth_trx(std::move(trx))}, get_ctx_block_num(loc->blk_n), std::move(params)));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Debug::trace_replayBlockTransactions(const std::string& block_num, const Json::Value& trace_params) {
  Json::Value res;
  try {
    const auto block = parse_blk_num(block_num);
    auto params = parse_tracking_parms(trace_params);
    if (auto node = full_node_.lock()) {
      auto transactions = node->getDB()->getPeriodTransactions(block);
      if (!transactions.has_value() || transactions->empty()) {
        res["status"] = "Block has no transactions";
        return res;
      }
      // TODO[2495]: remove after a proper fox of transactions ordering in PeriodData
      PbftManager::reorderTransactions(*transactions);
      std::vector<state_api::EVMTransaction> trxs;
      trxs.reserve(transactions->size());
      std::transform(transactions->begin(), transactions->end(), std::back_inserter(trxs),
                     [this](auto t) { return to_eth_trx(std::move(t)); });
      return util::readJsonFromString(
          node->getFinalChain()->trace(std::move(trxs), get_ctx_block_num(block), std::move(params)));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

template <class S, class FN>
Json::Value transformToJsonParallel(const S& source, FN op) {
  if (source.empty()) {
    return Json::Value(Json::arrayValue);
  }
  static util::ThreadPool executor{std::thread::hardware_concurrency() / 2};

  Json::Value out(Json::arrayValue);
  out.resize(source.size());
  std::atomic_uint processed = 0;
  for (unsigned i = 0; i < source.size(); ++i) {
    executor.post([&, i]() {
      out[i] = op(source[i]);
      ++processed;
    });
  }

  while (true) {
    if (processed == source.size()) {
      break;
    }
  }
  return out;
}

Json::Value mergeJsons(Json::Value&& o1, Json::Value&& o2) {
  for (auto itr = o2.begin(); itr != o2.end(); ++itr) {
    o1[itr.key().asString()] = *itr;
  }
  return o1;
}

Json::Value Debug::debug_getPeriodTransactionsWithReceipts(const std::string& _period) {
  try {
    auto node = full_node_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }
    auto final_chain = node->getFinalChain();
    auto period = dev::jsToInt(_period);
    auto block_hash = final_chain->block_hash(period);
    auto trxs = node->getDB()->getPeriodTransactions(period);
    if (!trxs.has_value()) {
      return Json::Value(Json::arrayValue);
    }

    // TODO[2495]: remove after a proper fox of transactions ordering in PeriodData
    PbftManager::reorderTransactions(*trxs);

    return transformToJsonParallel(*trxs, [&final_chain, &block_hash](const auto& trx) {
      auto hash = trx->getHash();
      auto r = final_chain->transaction_receipt(hash);
      auto location =
          rpc::eth::ExtendedTransactionLocation{{*final_chain->transaction_location(hash), *block_hash}, hash};
      auto transaction = rpc::eth::LocalisedTransaction{trx, location};
      auto receipt = rpc::eth::LocalisedTransactionReceipt{*r, location, trx->getSender(), trx->getReceiver()};
      auto receipt_json = rpc::eth::toJson(receipt);
      receipt_json.removeMember("transactionHash");

      return mergeJsons(rpc::eth::toJson(transaction), std::move(receipt_json));
    });
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Debug::debug_getPeriodDagBlocks(const std::string& _period) {
  try {
    auto node = full_node_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto period = dev::jsToInt(_period);
    auto dags = node->getDB()->getFinalizedDagBlockByPeriod(period);

    return transformToJsonParallel(dags, [&period](const auto& dag) {
      auto block_json = dag->getJson();
      block_json["period"] = toJS(period);
      return block_json;
    });
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Debug::debug_getPreviousBlockCertVotes(const std::string& _period) {
  try {
    auto node = full_node_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto final_chain = node->getFinalChain();
    auto vote_manager = node->getVoteManager();

    Json::Value res(Json::objectValue);

    auto period = dev::jsToInt(_period);
    auto votes = node->getDB()->getPeriodCertVotes(period);
    if (votes.empty()) {
      return res;
    }

    const auto votes_period = votes.front()->getPeriod();
    const uint64_t total_dpos_votes_count = final_chain->dpos_eligible_total_vote_count(votes_period - 1);
    res["total_votes_count"] = total_dpos_votes_count;
    res["votes"] = transformToJsonParallel(votes, [&](const auto& vote) {
      vote_manager->validateVote(vote);
      return vote->toJSON();
    });
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

state_api::Tracing Debug::parse_tracking_parms(const Json::Value& json) const {
  state_api::Tracing ret;
  if (!json.isArray() || json.empty()) {
    throw InvalidTracingParams();
  }
  for (const auto& obj : json) {
    if (obj.asString() == "trace") ret.trace = true;
    // Disabled for now
    // if (obj.asString() == "stateDiff") ret.stateDiff = true;
    if (obj.asString() == "vmTrace") ret.vmTrace = true;
  }
  return ret;
}

state_api::EVMTransaction Debug::to_eth_trx(std::shared_ptr<Transaction> t) const {
  return state_api::EVMTransaction{
      t->getSender(), t->getGasPrice(), t->getReceiver(), t->getNonce(), t->getValue(), t->getGas(), t->getData(),
  };
}

state_api::EVMTransaction Debug::to_eth_trx(const Json::Value& json, EthBlockNumber blk_num) {
  state_api::EVMTransaction trx;
  if (!json.isObject() || json.empty()) {
    return trx;
  }

  if (!json["from"].empty()) {
    trx.from = to_address(json["from"].asString());
  } else {
    trx.from = ZeroAddress;
  }

  if (!json["to"].empty() && json["to"].asString() != "0x" && !json["to"].asString().empty()) {
    trx.to = to_address(json["to"].asString());
  }

  if (!json["value"].empty()) {
    trx.value = jsToU256(json["value"].asString());
  }

  if (!json["gas"].empty()) {
    trx.gas = jsToInt(json["gas"].asString());
  } else {
    trx.gas = kGasLimit;
  }

  if (!json["gasPrice"].empty()) {
    trx.gas_price = jsToU256(json["gasPrice"].asString());
  } else {
    trx.gas_price = 0;
  }

  if (!json["data"].empty()) {
    trx.input = jsToBytes(json["data"].asString(), OnFailed::Throw);
  }
  if (!json["code"].empty()) {
    trx.input = jsToBytes(json["code"].asString(), OnFailed::Throw);
  }
  if (!json["nonce"].empty()) {
    trx.nonce = jsToU256(json["nonce"].asString());
  } else {
    if (auto node = full_node_.lock()) {
      trx.nonce = node->getFinalChain()->get_account(trx.from, blk_num).value_or(state_api::ZeroAccount).nonce;
    }
  }

  return trx;
}

EthBlockNumber Debug::parse_blk_num(const string& blk_num_str) {
  if (blk_num_str == "latest" || blk_num_str == "pending" || blk_num_str.empty()) {
    if (auto node = full_node_.lock()) {
      return node->getFinalChain()->last_block_number();
    }
  } else if (blk_num_str == "earliest") {
    return 0;
  }
  return jsToInt(blk_num_str);
}

Address Debug::to_address(const string& s) const {
  try {
    if (auto b = fromHex(s.substr(0, 2) == "0x" ? s.substr(2) : s, WhenError::Throw); b.size() == Address::size) {
      return Address(b);
    }
  } catch (BadHexCharacter&) {
  }
  throw InvalidAddress();
}

std::pair<std::shared_ptr<Transaction>, std::optional<final_chain::TransactionLocation>>
Debug::get_transaction_with_location(const std::string& transaction_hash) const {
  if (auto node = full_node_.lock()) {
    const auto hash = jsToFixed<32>(transaction_hash);
    return {node->getDB()->getTransaction(hash), node->getFinalChain()->transaction_location(hash)};
  }
  return {};
}

}  // namespace taraxa::net