#include "Taraxa.h"

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libp2p/Common.h>

#include <algorithm>

#include "dag/dag_manager.hpp"
#include "json/reader.h"
#include "multiproofs.hpp"
#include "network/rpc/eth/data.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace taraxa;
using namespace ::taraxa::final_chain;

namespace taraxa::net {

Taraxa::Taraxa(const std::shared_ptr<FullNode>& _full_node) : full_node_(_full_node) {
  Json::CharReaderBuilder builder;
  auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());

  bool parsingSuccessful = reader->parse(kVersionJson, kVersionJson + strlen(kVersionJson), &version, nullptr);
  assert(parsingSuccessful);
}

string Taraxa::taraxa_protocolVersion() { return toJS(TARAXA_NET_VERSION); }

Json::Value Taraxa::taraxa_getVersion() { return version; }

string Taraxa::taraxa_dagBlockLevel() {
  try {
    auto node = tryGetNode();
    return toJS(node->getDagManager()->getMaxLevel());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Taraxa::taraxa_dagBlockPeriod() {
  try {
    auto node = tryGetNode();
    return toJS(node->getDagManager()->getLatestPeriod());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Taraxa::NodePtr Taraxa::tryGetNode() {
  if (auto full_node = full_node_.lock()) {
    return full_node;
  }
  BOOST_THROW_EXCEPTION(jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getDagBlockByHash(const string& _blockHash, bool _includeTransactions) {
  try {
    auto node = tryGetNode();
    auto block = node->getDagManager()->getDagBlock(blk_hash_t(_blockHash));
    if (block) {
      auto block_json = block->getJson();
      auto period = node->getPbftManager()->getDagBlockPeriod(block->getHash());
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : block->getTrxs()) {
          block_json["transactions"].append(node->getTransactionManager()->getTransaction(t)->toJSON());
        }
      }
      return block_json;
    }
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
  return Json::Value();
}

std::string Taraxa::taraxa_pbftBlockHashByPeriod(const std::string& _period) {
  try {
    auto node = tryGetNode();
    auto db = node->getDB();
    auto blk = db->getPbftBlock(dev::jsToInt(_period));
    if (!blk.has_value()) {
      return {};
    }
    return toJS(blk->getBlockHash());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getScheduleBlockByPeriod(const std::string& _period) {
  try {
    auto period = dev::jsToInt(_period);
    auto node = tryGetNode();
    auto db = node->getDB();
    auto blk = db->getPbftBlock(period);
    if (!blk.has_value()) {
      return Json::Value();
    }
    return PbftBlock::toJson(*blk, db->getFinalizedDagBlockHashesByPeriod(period));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getDagBlockByLevel(const string& _blockLevel, bool _includeTransactions) {
  try {
    auto node = tryGetNode();
    auto blocks = node->getDB()->getDagBlocksAtLevel(dev::jsToInt(_blockLevel), 1);
    auto res = Json::Value(Json::arrayValue);
    for (auto const& b : blocks) {
      auto block_json = b->getJson();
      auto period = node->getPbftManager()->getDagBlockPeriod(b->getHash());
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : b->getTrxs()) {
          block_json["transactions"].append(node->getTransactionManager()->getTransaction(t)->toJSON());
        }
      }
      res.append(block_json);
    }
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getConfig() { return enc_json(tryGetNode()->getConfig().genesis); }

Json::Value Taraxa::taraxa_getChainStats() {
  Json::Value res;
  if (auto node = full_node_.lock()) {
    res["pbft_period"] = Json::UInt64(node->getPbftChain()->getPbftChainSize());
    res["dag_blocks_executed"] = Json::UInt64(node->getDB()->getNumBlockExecuted());
    res["transactions_executed"] = Json::UInt64(node->getDB()->getNumTransactionExecuted());
  }

  return res;
}

EthBlockNumber Taraxa::parse_blk_num(const string& blk_num_str) {
  auto ret = parse_blk_num_specific(blk_num_str);
  if (ret) {
    return *ret;
  } else if (auto node = full_node_.lock()) {
    return node->getFinalChain()->last_block_number();
  }
}

EthBlockNumber Taraxa::get_block_number_from_json(const Json::Value& json) {
  if (json.isObject()) {
    if (!json["blockNumber"].empty()) {
      return parse_blk_num(json["blockNumber"].asString());
    }
    if (!json["blockHash"].empty()) {
      if (auto node = full_node_.lock()) {
        if (auto ret = node->getFinalChain()->block_number(dev::jsToFixed<32>(json["blockHash"].asString()))) {
          return *ret;
        }
      }
      throw std::runtime_error("Resource not found");
    }
  }
  return parse_blk_num(json.asString());
}

Json::Value Taraxa::taraxa_getMultiProof(const string& _address, const Json::Value& _keys, const Json::Value& _block) {
  const auto block_number = get_block_number_from_json(_block);
  std::vector<h256> keys;
  keys.reserve(_keys.size());
  std::transform(_keys.begin(), _keys.end(), std::back_inserter(keys),
                 [](const auto& k) { return dev::jsToFixed<32>(k.asString()); });
  if (auto node = full_node_.lock()) {
    auto proof = node->getFinalChain()->get_proof(block_number, addr_t(_address), keys);
    auto proof_json = taraxa::net::rpc::eth::toJson(proof);
    const auto [acc_index, trie] = generate_multiproof(proof);
    Json::Value res(Json::objectValue);
    res["multiproof"] = toJson(trie);
    res["accountMap"] = taraxa::net::rpc::eth::toJsonArray(acc_index);
    proof_json["address"] = _address;
    proof_json["storageProof"] = res;
    return proof_json;
  }
  return Json::Value();
}

}  // namespace taraxa::net