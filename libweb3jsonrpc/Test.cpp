#include "Test.h"
#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>
#include "../pbft_manager.hpp"
#include "core_tests/create_samples.hpp"
#include "types.hpp"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::rpc;
using namespace jsonrpc;
using namespace taraxa;

Test::Test(std::shared_ptr<taraxa::FullNode> &_full_node)
    : full_node_(_full_node) {}

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
      node->insertBlock(std::move(blk));
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::insert_stamped_dag_block(const Json::Value &param1) {
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
      time_stamp_t stamp = param1["stamp"].asUInt64();

      DagBlock blk(pivot, 0, tips, {}, signature, hash, sender);
      res =
          blk.getJsonStr() + ("\n Block stamped at: " + std::to_string(stamp));
      node->insertBlock(std::move(blk));
      node->setDagBlockTimeStamp(hash, stamp);
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
      auto blk = node->getDagBlock(hash);
      if (!blk) {
        res = "Block not available \n";
      } else {
        time_stamp_t stamp = node->getDagBlockTimeStamp(hash);
        res = blk->getJsonStr() + "\ntime_stamp: " + std::to_string(stamp);
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_children(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      time_stamp_t stamp = param1["stamp"].asUInt64();
      std::vector<std::string> children;
      children = node->getDagBlockChildren(hash, stamp);
      for (auto const &child : children) {
        res = res.asString() + (child + '\n');
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_siblings(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      time_stamp_t stamp = param1["stamp"].asUInt64();
      std::vector<std::string> siblings;
      siblings = node->getDagBlockSiblings(hash, stamp);
      for (auto const &sibling : siblings) {
        res = res.asString() + (sibling + '\n');
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_tips(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      time_stamp_t stamp = param1["stamp"].asUInt64();
      std::vector<std::string> tips;
      tips = node->getDagBlockTips(hash, stamp);
      for (auto const &tip : tips) {
        res = res.asString() + (tip + '\n');
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_pivot_chain(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      time_stamp_t stamp = param1["stamp"].asUInt64();
      std::vector<std::string> pivot_chain;
      pivot_chain = node->getDagBlockPivotChain(hash, stamp);
      for (auto const &pivot : pivot_chain) {
        res = res.asString() + (pivot + '\n');
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_subtree(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t hash = blk_hash_t(param1["hash"].asString());
      time_stamp_t stamp = param1["stamp"].asUInt64();
      std::vector<std::string> subtree;
      subtree = node->getDagBlockSubtree(hash, stamp);
      for (auto const &v : subtree) {
        res = res.asString() + (v + '\n');
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_block_epfriend(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t from_hash = blk_hash_t(param1["from_hash"].asString());
      blk_hash_t to_hash = blk_hash_t(param1["to_hash"].asString());

      std::vector<std::string> epfriend;
      epfriend = node->getDagBlockEpFriend(from_hash, to_hash);
      for (auto const &v : epfriend) {
        res = res.asString() + (v + '\n');
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
      val_t nonce = val_t(param1["nonce"].asString());
      val_t value = val_t(param1["value"].asString());
      val_t gas_price = val_t(param1["gas_price"].asString());
      val_t gas = val_t(param1["gas"].asString());
      addr_t receiver = addr_t(param1["receiver"].asString());
      bytes data;
      // get trx receiving time stamp
      auto now = getCurrentTimeMilliSeconds();
      taraxa::Transaction trx(nonce, value, gas_price, gas, receiver, data, sk);
      LOG(log_time) << "Transaction " << trx.getHash()
                    << " received at: " << now;
      node->insertTransaction(trx);
      res = trx.getJsonStr();
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
      val_t nonce = val_t(param1["nonce"].asString());
      addr_t receiver = addr_t(param1["receiver"].asString());
      bytes data;
      // get trx receiving time stamp
      for (auto i = 0; i < number; ++i) {
        auto now = getCurrentTimeMilliSeconds();
        val_t value = val_t(100);
        auto trx = taraxa::Transaction(val_t(i)+nonce, value, val_t(1000),
                                       taraxa::samples::TEST_TX_GAS_LIMIT,
                                       receiver, data, node->getSecretKey());
        LOG(log_time) << "Transaction " << trx.getHash()
                      << " received at: " << now;
        node->insertTransaction(trx);
        thisThreadSleepForMicroSeconds(delay);
      }
      res = "Number of " + std::to_string(number) + " created";
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
      LOG(log_time) << "Number of proposed block " << num_prop_block
                    << std::endl;
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::send_pbft_schedule_block(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t prev_blk = blk_hash_t(param1["prev_pivot"].asString());
      taraxa::sig_t sig = taraxa::sig_t(param1["sig"].asString());
      vec_blk_t blk_order = asVector<blk_hash_t>(param1["blk_order"]);
      std::vector<size_t> trx_sizes;
      for (auto &item : param1["trx_sizes"]) {
        trx_sizes.push_back((item.asUInt64()));
      }
      assert(blk_order.size() == trx_sizes.size());
      std::vector<size_t> trx_modes;
      for (auto &item : param1["trx_modes"]) {
        trx_modes.push_back((item.asUInt64()));
      }

      std::vector<std::vector<uint>> vec_trx_modes(trx_sizes.size());
      size_t count = 0;
      for (auto i = 0; i < trx_sizes.size(); ++i) {
        for (auto j = 0; j < trx_sizes[i]; ++j) {
          vec_trx_modes[i].emplace_back(trx_modes[count++]);
        }
      }

      TrxSchedule sche(blk_order, vec_trx_modes);
      ScheduleBlock sche_blk(prev_blk, sche);
      res = sche_blk.getJsonStr();
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
      auto bal = node->getBalance(addr);
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
      auto peer = node->getPeerCount();
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
      auto peers = node->getAllPeers();
      for (auto const &peer : peers) {
        res = res.asString() + peer.toString() + "\n";
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::node_stop() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      node->stop();
      res = "Taraxa node stopped ...\n";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::node_reset() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      node->reset();
      res = "Taraxa node reset ...\n";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::node_start() {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      node->start(false /*destroy_db*/);
      res = "Taraxa node start ...\n";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::should_speak(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t blockhash = blk_hash_t(param1["blockhash"].asString());
      PbftVoteTypes type = static_cast<PbftVoteTypes>(param1["type"].asInt());
      uint64_t period = param1["period"].asUInt64();
      size_t step = param1["step"].asUInt();
      if (node->shouldSpeak(type, period, step)) {
        res = "True";
      } else {
        res = "False";
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::place_vote(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      blk_hash_t blockhash = blk_hash_t(param1["blockhash"].asString());
      PbftVoteTypes type = static_cast<PbftVoteTypes>(param1["type"].asInt());
      uint64_t period = param1["period"].asUInt64();
      size_t step = param1["step"].asUInt();

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

Json::Value Test::get_votes(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      uint64_t period = param1["period"].asUInt64();

      std::vector<Vote> votes = node->getVotes(period);
      VoteManager vote_mgr;
      res = vote_mgr.getJsonStr(votes);
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
      node->drawGraph(filename);
      res = "Dag is drwan as " + filename + " on the server side ...";
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_transaction_count(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getTransactionStatusCount();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_executed_trx_count(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getNumTransactionExecuted();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_executed_blk_count(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getNumBlockExecuted();
      res["value"] = Json::UInt64(count);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_dag_size(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      auto count = node->getNumVerticesInDag();
      res["value"] =
          std::to_string(count.first) + " , " + std::to_string(count.second);
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}
