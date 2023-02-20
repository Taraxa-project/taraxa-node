#include "Debug.h"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "common/jsoncpp.hpp"
#include "final_chain/state_api_data.hpp"

using namespace std;
using namespace dev;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {
Json::Value Debug::debug_traceTransaction(const std::string& transaction_hash) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      const auto hash = jsToFixed<32>(transaction_hash);
      const auto trx = node->getDB()->getTransaction(hash);
      if (!trx) {
        res["status"] = "Transaction not found";
        return res;
      }
      const auto loc = node->getFinalChain()->transaction_location(hash);
      if (!loc) {
        res["status"] = "Transaction not found";
        return res;
      }
      return util::readJsonFromString(node->getFinalChain()->trace_trx(to_eth_trx(trx), loc->blk_n));
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
      return util::readJsonFromString(node->getFinalChain()->trace_trx(std::move(trx), block));
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
    auto trx = to_eth_trx(call_params, block);
    auto params = parse_tracking_parms(trace_params);
    if (auto node = full_node_.lock()) {
      return util::readJsonFromString(node->getFinalChain()->trace_trx(std::move(trx), block, std::move(params)));
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

state_api::Tracing Debug::parse_tracking_parms(const Json::Value& json) const {
  state_api::Tracing ret;
  if (!json.isArray() || json.empty()) {
    throw InvalidTracingParams();
  }
  for (const auto& obj : json) {
    if (obj.asString() == "trace") ret.trace = true;
    if (obj.asString() == "stateDiff") ret.stateDiff = true;
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
    trx.from = toAddress(json["from"].asString());
  } else {
    trx.from = ZeroAddress;
  }

  if (!json["to"].empty() && json["to"].asString() != "0x" && !json["to"].asString().empty()) {
    trx.to = toAddress(json["to"].asString());
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

Address Debug::toAddress(const string& s) const {
  try {
    if (auto b = fromHex(s.substr(0, 2) == "0x" ? s.substr(2) : s, WhenError::Throw); b.size() == Address::size) {
      return Address(b);
    }
  } catch (BadHexCharacter&) {
  }
  throw InvalidAddress();
}

}  // namespace taraxa::net