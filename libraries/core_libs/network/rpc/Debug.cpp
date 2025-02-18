#include "Debug.h"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "common/jsoncpp.hpp"
#include "common/thread_pool.hpp"
#include "final_chain/final_chain.hpp"
#include "final_chain/state_api_data.hpp"
#include "network/rpc/eth/data.hpp"
#include "transaction/transaction.hpp"
#include "vote_manager/vote_manager.hpp"

using namespace std;
using namespace dev;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

Json::Value Debug::debug_traceCall(const Json::Value& call_params, const std::string& blk_num) {
  Json::Value res;
  const auto block = parse_blk_num(blk_num);
  auto trx = to_eth_trx(call_params, block);
  if (auto node = app_.lock()) {
    return util::readJsonFromString(node->getFinalChain()->trace({}, {std::move(trx)}, block));
  }
  return res;
}

Json::Value Debug::trace_call(const Json::Value& call_params, const Json::Value& trace_params,
                              const std::string& blk_num) {
  Json::Value res;
  const auto block = parse_blk_num(blk_num);
  auto params = parse_tracking_parms(trace_params);
  if (auto node = app_.lock()) {
    return util::readJsonFromString(
        node->getFinalChain()->trace({}, {to_eth_trx(call_params, block)}, block, std::move(params)));
  }
  return res;
}

std::tuple<std::vector<state_api::EVMTransaction>, state_api::EVMTransaction, uint64_t>
Debug::get_transaction_with_state(const std::string& transaction_hash) {
  auto node = app_.lock();
  if (!node) {
    return {};
  }

  auto final_chain = node->getFinalChain();
  auto loc = final_chain->transactionLocation(jsToFixed<32>(transaction_hash));
  if (!loc) {
    throw std::runtime_error("Transaction not found");
  }
  auto block_transactions = final_chain->transactions(loc->period);

  auto state_trxs = SharedTransactions(block_transactions.begin(), block_transactions.begin() + loc->position);

  return {to_eth_trxs(state_trxs), to_eth_trx(block_transactions[loc->position]), loc->period};
}
Json::Value Debug::debug_traceTransaction(const std::string& transaction_hash) {
  Json::Value res;
  auto [state_trxs, trx, period] = get_transaction_with_state(transaction_hash);
  if (auto node = app_.lock()) {
    return util::readJsonFromString(node->getFinalChain()->trace({}, {trx}, period));
  }
  return res;
}

Json::Value Debug::trace_replayTransaction(const std::string& transaction_hash, const Json::Value& trace_params) {
  Json::Value res;
  auto params = parse_tracking_parms(trace_params);
  auto [state_trxs, trx, period] = get_transaction_with_state(transaction_hash);
  if (auto node = app_.lock()) {
    return util::readJsonFromString(node->getFinalChain()->trace(state_trxs, {trx}, period, params));
  }
  return res;
}

bool only_transfers(const SharedTransactions& trxs) {
  return std::all_of(trxs.begin(), trxs.end(), [](const SharedTransaction& trx) {
    return trx->getReceiver().has_value() && trx->getData().empty() && trx->getGas() <= 22000;
  });
}

Json::Value Debug::trace_replayBlockTransactions(const std::string& block_num, const Json::Value& trace_params) {
  Json::Value res;
  const auto block = parse_blk_num(block_num);
  auto params = parse_tracking_parms(trace_params);
  if (auto node = app_.lock()) {
    auto transactions = node->getDB()->getPeriodTransactions(block);
    if (!transactions.has_value() || transactions->empty()) {
      return Json::Value(Json::arrayValue);
    }
    if (only_transfers(*transactions)) {
      return Json::Value(Json::arrayValue);
    }
    std::vector<state_api::EVMTransaction> trxs = to_eth_trxs(*transactions);
    return util::readJsonFromString(node->getFinalChain()->trace({}, std::move(trxs), block, std::move(params)));
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
      out[i] = op(source[i], i);
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
    auto node = app_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }
    auto final_chain = node->getFinalChain();
    auto period = dev::jsToInt(_period);
    auto block_hash = final_chain->blockHash(period);
    auto trxs = node->getDB()->getPeriodTransactions(period);
    if (!trxs.has_value()) {
      return Json::Value(Json::arrayValue);
    }

    return transformToJsonParallel(*trxs, [&final_chain, &block_hash, &period](const auto& trx, auto index) {
      auto hash = trx->getHash();
      auto r = final_chain->transactionReceipt(hash);
      auto location = rpc::eth::ExtendedTransactionLocation{{TransactionLocation{period, index}, *block_hash}, hash};
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
    auto node = app_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto period = dev::jsToInt(_period);
    auto dags = node->getDB()->getFinalizedDagBlockByPeriod(period);

    return transformToJsonParallel(dags, [&period](const auto& dag, auto) {
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
    auto node = app_.lock();
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
    const uint64_t total_dpos_votes_count = final_chain->dposEligibleTotalVoteCount(votes_period - 1);
    res["total_votes_count"] = total_dpos_votes_count;
    res["votes"] = transformToJsonParallel(votes, [&](const auto& vote, auto) {
      vote_manager->validateVote(vote);
      return vote->toJSON();
    });
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Debug::debug_dposValidatorTotalStakes(const std::string& _period) {
  try {
    auto node = app_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto final_chain = node->getFinalChain();
    auto vote_manager = node->getVoteManager();

    auto period = dev::jsToInt(_period);
    auto validatorsStakes = final_chain->dposValidatorsTotalStakes(period);

    Json::Value res(Json::arrayValue);

    for (auto const& validatorStake : validatorsStakes) {
      Json::Value validatorStakeJson(Json::objectValue);
      validatorStakeJson["address"] = "0x" + validatorStake.addr.toString();
      validatorStakeJson["total_stake"] = validatorStake.stake.str();
      res.append(validatorStakeJson);
    }
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Debug::debug_dposTotalAmountDelegated(const std::string& _period) {
  try {
    auto node = app_.lock();
    if (!node) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto final_chain = node->getFinalChain();

    auto period = dev::jsToInt(_period);
    auto totalAmountDelegated = final_chain->dposTotalAmountDelegated(period);

    return toJS(totalAmountDelegated);
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

std::vector<state_api::EVMTransaction> Debug::to_eth_trxs(const std::vector<std::shared_ptr<Transaction>>& trxs) {
  std::vector<state_api::EVMTransaction> eth_trxs;
  eth_trxs.reserve(trxs.size());
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(eth_trxs),
                 [this](auto t) { return to_eth_trx(std::move(t)); });
  return eth_trxs;
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
    if (auto node = app_.lock()) {
      trx.nonce = node->getFinalChain()->getAccount(trx.from, blk_num).value_or(state_api::ZeroAccount).nonce;
    }
  }

  return trx;
}

EthBlockNumber Debug::parse_blk_num(const std::string& blk_num_str) {
  if (blk_num_str == "latest" || blk_num_str == "pending" || blk_num_str.empty()) {
    if (auto node = app_.lock()) {
      return node->getFinalChain()->lastBlockNumber();
    }
  } else if (blk_num_str == "earliest") {
    return 0;
  }
  return jsToInt(blk_num_str);
}

Address Debug::to_address(const std::string& s) const {
  try {
    if (auto b = fromHex(s.substr(0, 2) == "0x" ? s.substr(2) : s, WhenError::Throw); b.size() == Address::size) {
      return Address(b);
    }
  } catch (BadHexCharacter&) {
  }
  throw InvalidAddress();
}

}  // namespace taraxa::net