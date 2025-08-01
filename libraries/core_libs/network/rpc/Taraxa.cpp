#include "Taraxa.h"

#include <json/reader.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libp2p/Common.h>

#include "config/version.hpp"
#include "dag/dag_manager.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"

using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace taraxa;
using namespace ::taraxa::final_chain;

namespace taraxa::net {

Taraxa::Taraxa(std::shared_ptr<AppBase> app) : app_(app) {
  Json::CharReaderBuilder builder;
  auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());

  bool parsingSuccessful = reader->parse(kVersionJson, kVersionJson + strlen(kVersionJson), &version, nullptr);
  assert(parsingSuccessful);
}

string Taraxa::taraxa_protocolVersion() { return toJS(TARAXA_NET_VERSION); }

Json::Value Taraxa::taraxa_getVersion() { return version; }

string Taraxa::taraxa_dagBlockLevel() {
  try {
    auto app = tryGetApp();
    return toJS(app->getDagManager()->getMaxLevel());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

string Taraxa::taraxa_dagBlockPeriod() {
  try {
    auto app = tryGetApp();
    return toJS(app->getDagManager()->getLatestPeriod());
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

std::shared_ptr<AppBase> Taraxa::tryGetApp() {
  if (auto app = app_.lock()) {
    return app;
  }
  BOOST_THROW_EXCEPTION(jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR));
}

Json::Value Taraxa::taraxa_getDagBlockByHash(const string& _blockHash, bool _includeTransactions) {
  try {
    auto app = tryGetApp();
    auto block = app->getDagManager()->getDagBlock(blk_hash_t(_blockHash));
    if (block) {
      auto block_json = block->getJson();
      auto period = app->getPbftManager()->getDagBlockPeriod(block->getHash());
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : block->getTrxs()) {
          block_json["transactions"].append(app->getTransactionManager()->getTransaction(t)->toJSON());
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
    auto app = tryGetApp();
    auto db = app->getDB();
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
    auto app = tryGetApp();
    auto period = dev::jsToInt(_period);
    auto db = app->getDB();
    auto blk = db->getPbftBlock(period);
    if (!blk.has_value()) {
      return Json::Value();
    }
    return PbftBlock::toJson(*blk, db->getFinalizedDagBlockHashesByPeriod(period));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getNodeVersions() {
  try {
    auto app = tryGetApp();
    auto db = app->getDB();
    auto period = app->getFinalChain()->lastBlockNumber();
    const uint64_t max_blocks_to_process = 6000;
    std::map<addr_t, std::string> node_version_map;
    std::multimap<std::string, std::pair<addr_t, uint64_t>> version_node_map;
    std::map<std::string, std::pair<uint32_t, uint32_t>> version_count;
    for (uint64_t i = period; i > 0 && period - i < max_blocks_to_process; i--) {
      auto blk = db->getPbftBlock(i);
      if (!blk.has_value()) {
        break;
      }
      if (!node_version_map.contains(blk->getBeneficiary())) {
        node_version_map[blk->getBeneficiary()] = blk->getExtraData()->getJson()["major_version"].asString() + "." +
                                                  blk->getExtraData()->getJson()["minor_version"].asString() + "." +
                                                  blk->getExtraData()->getJson()["patch_version"].asString();
      }
    }

    auto total_vote_count = app->getFinalChain()->dposEligibleTotalVoteCount(period);
    for (auto nv : node_version_map) {
      auto vote_count = app->getFinalChain()->dposEligibleVoteCount(period, nv.first);
      version_node_map.insert({nv.second, {nv.first, vote_count}});
      version_count[nv.second].first++;
      version_count[nv.second].second += vote_count;
    }

    Json::Value res;
    res["nodes"] = Json::Value(Json::arrayValue);
    for (auto vn : version_node_map) {
      Json::Value node_json;
      node_json["node"] = vn.second.first.toString();
      node_json["version"] = vn.first;
      node_json["vote_count"] = vn.second.second;
      res["nodes"].append(node_json);
    }
    res["versions"] = Json::Value(Json::arrayValue);
    for (auto vc : version_count) {
      Json::Value version_json;
      version_json["version"] = vc.first;
      version_json["node_count"] = vc.second.first;
      version_json["vote_percentage"] = vc.second.second * 100 / total_vote_count;
      res["versions"].append(version_json);
    }
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getDagBlockByLevel(const string& _blockLevel, bool _includeTransactions) {
  try {
    auto app = tryGetApp();
    auto blocks = app->getDB()->getDagBlocksAtLevel(dev::jsToInt(_blockLevel), 1);
    auto res = Json::Value(Json::arrayValue);
    for (auto const& b : blocks) {
      auto block_json = b->getJson();
      auto period = app->getPbftManager()->getDagBlockPeriod(b->getHash());
      if (period.first) {
        block_json["period"] = toJS(period.second);
      } else {
        block_json["period"] = "-0x1";
      }
      if (_includeTransactions) {
        block_json["transactions"] = Json::Value(Json::arrayValue);
        for (auto const& t : b->getTrxs()) {
          block_json["transactions"].append(app->getTransactionManager()->getTransaction(t)->toJSON());
        }
      }
      res.append(block_json);
    }
    return res;
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getConfig() { return enc_json(tryGetApp()->getConfig().genesis); }

Json::Value Taraxa::taraxa_getChainStats() {
  Json::Value res;
  if (auto app = app_.lock()) {
    res["pbft_period"] = Json::UInt64(app->getFinalChain()->lastBlockNumber());
    res["dag_blocks_executed"] = Json::UInt64(app->getDB()->getNumBlockExecuted());
    res["transactions_executed"] = Json::UInt64(app->getDB()->getNumTransactionExecuted());
  }

  return res;
}

std::string Taraxa::taraxa_yield(const std::string& _period) {
  try {
    auto app = app_.lock();
    if (!app) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto period = dev::jsToInt(_period);
    return toJS(app->getFinalChain()->dposYield(period));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

std::string Taraxa::taraxa_totalSupply(const std::string& _period) {
  try {
    auto app = app_.lock();
    if (!app) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    auto period = dev::jsToInt(_period);
    return toJS(app->getFinalChain()->dposTotalSupply(period));
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

Json::Value Taraxa::taraxa_getPillarBlockData(const std::string& pillar_block_period, bool include_signatures) {
  try {
    auto app = app_.lock();
    if (!app) {
      BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
    }

    const auto pbft_period = dev::jsToInt(pillar_block_period);
    if (!app->getConfig().genesis.state.hardforks.ficus_hf.isPillarBlockPeriod(pbft_period)) {
      return {};
    }

    const auto pillar_block = app->getDB()->getPillarBlock(pbft_period);
    if (!pillar_block) {
      return {};
    }

    const auto& pillar_votes = app->getDB()->getPeriodPillarVotes(pbft_period + 1);
    if (pillar_votes.empty()) {
      return {};
    }

    return pillar_chain::PillarBlockData{pillar_block, pillar_votes}.getJson(include_signatures);
  } catch (...) {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS));
  }
}

}  // namespace taraxa::net