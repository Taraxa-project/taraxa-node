#include "Taraxa.h"

#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libp2p/Common.h>

#include <algorithm>

#include "dag/dag_manager.hpp"
#include "json/reader.h"
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
  try {
    if (auto node = full_node_.lock()) {
      res["pbft_period"] = Json::UInt64(node->getPbftChain()->getPbftChainSize());
      res["dag_blocks_executed"] = Json::UInt64(node->getDB()->getNumBlockExecuted());
      res["transactions_executed"] = Json::UInt64(node->getDB()->getNumTransactionExecuted());
    }
  } catch (std::exception& e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value mergeJsons(Json::Value&& o1, Json::Value&& o2) {
  for (auto itr = o2.begin(); itr != o2.end(); ++itr) {
    o1[itr.key().asString()] = *itr;
  }
  return o1;
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

Json::Value Taraxa::taraxa_getPeriodTransactionsWithReceipts(const std::string& _period) {
  try {
    auto node = tryGetNode();
    auto final_chain = node->getFinalChain();
    auto period = dev::jsToInt(_period);
    auto block_hash = final_chain->block_hash(period);
    auto trxs = node->getDB()->getPeriodTransactions(period);
    if (!trxs.has_value()) {
      return Json::Value(Json::arrayValue);
    }

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

Json::Value Taraxa::taraxa_getPeriodDagBlocks(const std::string& _period) {
  try {
    auto period = dev::jsToInt(_period);
    auto node = tryGetNode();
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

}  // namespace taraxa::net