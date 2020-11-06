#include "Taraxa.h"

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libethcore/CommonJS.h>
#include <libethereum/TransactionReceipt.h>
#include <libp2p/Common.h>

#include <csignal>

#include "dag.hpp"
#include "dag_block.hpp"
#include "pbft_manager.hpp"
#include "transaction_manager.hpp"

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace taraxa;

namespace taraxa::net {

Taraxa::Taraxa(std::shared_ptr<FullNode> const& _full_node) : full_node_(_full_node) {}

string Taraxa::taraxa_protocolVersion() { return toJS(dev::p2p::c_protocolVersion); }

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

Json::Value Taraxa::taraxa_getDagBlockByHash(string const& _blockHash, bool _includeTransactions) {
  try {
    auto node = tryGetNode();
    auto block = node->getBlockManager()->getDagBlock(blk_hash_t(_blockHash));
    if (block) {
      auto block_json = block->getJson();
      auto period = node->getPbftManager()->getDagBlockPeriod(blk_hash_t(block->getHash()));
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : block->getTrxs()) {
          block_json["transactions"].append(node->getTransactionManager()->getTransaction(t)->first.toJSON());
        }
      }
      return block_json;
    }
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
  return Json::Value();
}

Json::Value Taraxa::taraxa_getScheduleBlockByPeriod(std::string const& _period) {
  try {
    auto node = tryGetNode();
    auto db = node->getDB();
    auto blk_h = db->getPeriodPbftBlock(std::stoull(_period, 0, 16));
    if (!blk_h) {
      return Json::Value();
    }
    auto blk = node->getPbftChain()->getPbftBlockInChain(*blk_h);
    return PbftBlock::toJson(blk, db->getFinalizedDagBlockHashesByAnchor(blk.getPivotDagBlockHash()));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getDagBlockByLevel(string const& _blockLevel, bool _includeTransactions) {
  try {
    auto node = tryGetNode();
    auto blocks = node->getDB()->getDagBlocksAtLevel(std::stoull(_blockLevel, 0, 16), 1);
    auto res = Json::Value(Json::arrayValue);
    for (auto const& b : blocks) {
      auto block_json = b->getJson();
      auto period = node->getPbftManager()->getDagBlockPeriod(blk_hash_t(b->getHash()));
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : b->getTrxs()) {
          block_json["transactions"].append(node->getTransactionManager()->getTransaction(t)->first.toJSON());
        }
      }
      res.append(block_json);
    }
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getConfig() { return enc_json(tryGetNode()->getConfig().chain); }

}  // namespace taraxa::net