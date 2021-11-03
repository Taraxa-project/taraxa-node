#include "Test.h"

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>

#include "common/types.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/network.hpp"
#include "node/node.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction_manager/transaction_manager.hpp"

using namespace std;
using namespace dev;
using namespace ::taraxa::final_chain;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

Json::Value Test::insert_dag_block(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t pivot = blk_hash_t(param1["pivot"].asString());
      vec_blk_t tips = asVector<blk_hash_t>(param1["tips"]);
      taraxa::sig_t signature = taraxa::sig_t(
          "777777777777777777777777777777777777777777777777777777777777777777"
          "777777777777777777777777777777777777777777777777777777777777777");
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      addr_t sender = addr_t(param1["sender"].asString());

      DagBlock blk(pivot, 0, tips, {}, signature, hash, sender);
      res = blk.getJsonStr();
      node->getDagManager()->addDagBlock(std::move(blk), {});
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      auto blk = node->getDagBlockManager()->getDagBlock(hash);
      if (!blk) {
        res = "Block not available \n";
      } else {
        res = node->getDagBlockManager()->getDagBlock(hash)->getJsonStr();
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::send_coin_transaction(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto &log_time = node->getTimeLogger();
      secret_t sk = secret_t(param1["secret"].asString());
      auto nonce = dev::jsToInt(param1["nonce"].asString());
      val_t value = val_t(param1["value"].asString());
      val_t gas_price = val_t(param1["gas_price"].asString());
      auto gas = dev::jsToInt(param1["gas"].asString());
      addr_t receiver = addr_t(param1["receiver"].asString());
      bytes data;
      // get trx receiving time stamp
      auto now = getCurrentTimeMilliSeconds();
      taraxa::Transaction trx(nonce, value, gas_price, gas, data, sk, receiver);
      LOG(log_time) << "Transaction " << trx.getHash() << " received at: " << now;
      node->getTransactionManager()->insertTransaction(trx);
      res = toHex(*trx.rlp());
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::create_test_coin_transactions(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto &log_time = node->getTimeLogger();
      uint delay = param1["delay"].asUInt();
      uint number = param1["number"].asUInt();
      auto nonce = dev::jsToInt(param1["nonce"].asString());
      addr_t receiver = addr_t(param1["receiver"].asString());
      secret_t sk = node->getSecretKey();
      if (!param1["secret"].empty() && !param1["secret"].asString().empty()) {
        sk = secret_t(param1["secret"].asString());
      }
      // get trx receiving time stamp
      uint i = 0;
      while (i < number) {
        auto now = getCurrentTimeMilliSeconds();
        val_t value = val_t(100);
        auto trx = taraxa::Transaction(i + nonce, value, 1000, 0, bytes(), sk, receiver);
        LOG(log_time) << "Transaction " << trx.getHash() << " received at: " << now;
        node->getTransactionManager()->insertTransaction(trx);
        thisThreadSleepForMicroSeconds(delay);
        i++;
      }
      res = "Creating " + std::to_string(number) + " transactions ...";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_num_proposed_blocks() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto &log_time = node->getTimeLogger();
      auto num_prop_block = node->getNumProposedBlocks();
      res["value"] = Json::UInt64(num_prop_block);
      LOG(log_time) << "Number of proposed block " << num_prop_block << std::endl;
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_account_address() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      addr_t addr = node->getAddress();
      res["value"] = addr.toString();
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_account_balance(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      addr_t addr = addr_t(param1["address"].asString());
      auto bal = node->getFinalChain()->getBalance(addr);
      if (!bal.second) {
        res["found"] = false;
        res["value"] = 0;
      } else {
        res["found"] = true;
        res["value"] = boost::lexical_cast<std::string>(bal.first);
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_peer_count() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto peer = node->getNetwork()->getPeerCount();
      res["value"] = Json::UInt64(peer);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_node_status() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      res["synced"] = !node->getNetwork()->pbft_syncing();
      res["syncing_seconds"] = Json::UInt64(node->getNetwork()->syncTimeSeconds());
      res["peer_count"] = Json::UInt64(node->getNetwork()->getPeerCount());
      res["node_count"] = Json::UInt64(node->getNetwork()->getNodeCount());
      res["blk_executed"] = Json::UInt64(node->getDB()->getNumBlockExecuted());
      res["blk_count"] = Json::UInt64(node->getDB()->getNumDagBlocks());
      res["trx_executed"] = Json::UInt64(node->getDB()->getNumTransactionExecuted());
      res["trx_count"] = Json::UInt64(node->getTransactionManager()->getTransactionCount());
      res["dag_level"] = Json::UInt64(node->getDagManager()->getMaxLevel());
      res["pbft_size"] = Json::UInt64(node->getPbftChain()->getPbftChainSize());
      res["pbft_sync_period"] = Json::UInt64(node->getPbftManager()->pbftSyncingPeriod());
      res["pbft_round"] = Json::UInt64(node->getPbftManager()->getPbftRound());
      res["dpos_total_votes"] = Json::UInt64(node->getPbftManager()->getDposTotalVotesCount());
      res["dpos_node_votes"] = Json::UInt64(node->getPbftManager()->getDposWeightedVotesCount());
      res["dpos_quorum"] = Json::UInt64(node->getPbftManager()->getTwoTPlusOne());
      res["pbft_sync_queue_size"] = Json::UInt64(node->getPbftManager()->syncBlockQueueSize());
      res["trx_pool_size"] = Json::UInt64(node->getTransactionManager()->getTransactionPoolSize());
      res["trx_nonfinalized_size"] = Json::UInt64(node->getTransactionManager()->getNonfinalizedTrxSize());
      res["blk_queue_unverified_size"] = Json::UInt64(node->getDagBlockManager()->getDagBlockQueueSize().first);
      res["blk_queue_verified_size"] = Json::UInt64(node->getDagBlockManager()->getDagBlockQueueSize().second);
      res["network"] = node->getNetwork()->getStatus();
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_packets_stats() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      return node->getNetwork()->getPacketsStats();
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_node_version() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      res["node_version"] = TARAXA_VERSION;
      res["db_version"] = getFormattedVersion({TARAXA_DB_MAJOR_VERSION, TARAXA_DB_MINOR_VERSION});
      res["network_version"] = std::to_string(TARAXA_NET_VERSION);
      ;
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_node_count() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto peer = node->getNetwork()->getNodeCount();
      res["value"] = Json::UInt64(peer);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_all_peers() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto peers = node->getNetwork()->getAllPeersIDs();
      for (auto const &peer : peers) {
        res = res.asString() + peer.toString() + "\n";
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_all_nodes() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto nodes = node->getNetwork()->getAllNodes();
      for (auto const &n : nodes) {
        res = res.asString() + n.id().toString() + " " + n.endpoint().address().to_string() + ":" +
              std::to_string(n.endpoint().tcpPort()) + "\n";
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_vote_weight(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      PbftVoteTypes type = static_cast<PbftVoteTypes>(param1["type"].asInt());
      uint64_t period = param1["period"].asUInt64();
      size_t step = param1["step"].asUInt();
      res = node->getPbftManager()->getVoteWeight(type, period, step);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::place_vote(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      // blk_hash_t blockhash = blk_hash_t(param1["blockhash"].asString());
      // PbftVoteTypes type = static_cast<PbftVoteTypes>(param1["type"].asInt());
      // uint64_t period = param1["period"].asUInt64();
      // size_t step = param1["step"].asUInt();

      // put vote into vote queue
      // node->placeVote(blockhash, type, period, step);
      // broadcast vote
      // node->broadcastVote(blockhash, type, period, step);

      res = "Place vote successfully";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_votes(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto vote_mgr = node->getVoteManager();

      auto verified_votes = vote_mgr->copyVerifiedVotes();
      auto unverified_votes = vote_mgr->getUnverifiedVotes();
      std::vector<std::shared_ptr<Vote>> votes;
      votes.reserve(verified_votes.size() + unverified_votes.size());
      votes.insert(votes.end(), verified_votes.begin(), verified_votes.end());
      votes.insert(votes.end(), unverified_votes.begin(), unverified_votes.end());

      res = vote_mgr->getJsonStr(votes);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::draw_graph(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      std::string filename = param1["filename"].asString();
      node->getDagManager()->drawGraph(filename);
      res = "Dag is drwan as " + filename + " on the server side ...";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_transaction_count(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getTransactionManager()->getTransactionCount();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_executed_trx_count(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getDB()->getNumTransactionExecuted();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_executed_blk_count(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getDB()->getNumBlockExecuted();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_size(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getDagManager()->getNumVerticesInDag();
      res["value"] = std::to_string(count.first) + " , " + std::to_string(count.second);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_blk_count(const Json::Value & /*param1*/) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getDB()->getNumDagBlocks();
      res["value"] = std::to_string(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_pbft_chain_size() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getPbftChain()->getPbftChainSize();
      res["value"] = std::to_string(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_pbft_chain_blocks(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto height_json = param1["height"];
      auto count_json = param1["count"];
      auto include_json = param1["include_json"];
      int height = 1;
      int count = 0;
      if (!count_json.isNull())
        count = std::stoi(count_json.asString());
      else
        count = node->getPbftChain()->getPbftChainSize();
      if (!height_json.isNull())
        height = std::stoi(height_json.asString());
      else
        height = node->getPbftChain()->getPbftChainSize() - count + 1;

      auto blocks = node->getPbftChain()->getPbftBlocksStr(height, count, include_json.isNull());
      res["value"] = Json::Value(Json::arrayValue);
      count = 0;
      for (auto const &b : blocks) {
        Json::Value block_json;
        block_json["height"] = height + count;
        count++;
        block_json["block"] = b;
        res["value"].append(block_json);
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_db_stats() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      for (auto const &col : taraxa::DB::Columns::all) {
        res[col.name()] = node->getDB()->getColumnSize(col);
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

}  // namespace taraxa::net
