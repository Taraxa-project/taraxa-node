#include "Test.h"

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>

#include "common/types.hpp"
#include "dag/dag_manager.hpp"
#include "network/network.hpp"
#include "node/node.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"

using namespace std;
using namespace dev;
using namespace ::taraxa::final_chain;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

Json::Value Test::get_sortition_change(const Json::Value &param1) {
  Json::Value res;
  try {
    if (auto node = full_node_.lock()) {
      uint64_t period = param1["period"].asUInt64();
      auto params_change = node->getDB()->getParamsChangeForPeriod(period);
      res["interval_efficiency"] = params_change->interval_efficiency;
      res["period"] = params_change->period;
      res["threshold_upper"] = params_change->vrf_params.threshold_upper;
      res["kThresholdUpperMinValue"] = params_change->vrf_params.kThresholdUpperMinValue;
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
      secret_t sk = secret_t(param1["secret"].asString());
      uint64_t nonce = 0;
      if (!param1["nonce"]) {
        auto acc = node->getFinalChain()->get_account(toAddress(sk));
        nonce = acc->nonce.convert_to<uint64_t>() + 1;
      } else {
        nonce = dev::jsToInt(param1["nonce"].asString());
      }
      val_t value = val_t(param1["value"].asString());
      val_t gas_price = val_t(param1["gas_price"].asString());
      auto gas = dev::jsToInt(param1["gas"].asString());
      addr_t receiver = addr_t(param1["receiver"].asString());
      bytes data;
      auto trx = std::make_shared<Transaction>(nonce, value, gas_price, gas, data, sk, receiver,
                                               node->getConfig().genesis.chain_id);
      if (auto [ok, err_msg] = node->getTransactionManager()->insertTransaction(trx); !ok) {
        res["status"] = err_msg;
      } else {
        res = toHex(trx->rlp());
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::send_coin_transactions(const Json::Value &param1) {
  Json::Value res;
  try {
    uint32_t inserted = 0;
    if (auto node = full_node_.lock()) {
      secret_t sk = secret_t(param1["secret"].asString());
      auto nonce = param1["nonce"].asUInt64();
      val_t value = val_t(param1["value"].asString());
      val_t gas_price = val_t(param1["gas_price"].asString());
      auto gas = dev::jsToInt(param1["gas"].asString());
      auto transactions_count = param1["transaction_count"].asUInt64();
      std::vector<addr_t> receivers;
      for (auto rec : param1["receiver"]) {
        receivers.emplace_back(addr_t(rec.asString()));
      }
      for (uint32_t i = 0; i < transactions_count; i++) {
        auto trx = std::make_shared<Transaction>(nonce, value, gas_price, gas, bytes(), sk,
                                                 receivers[i % receivers.size()], node->getConfig().genesis.chain_id);
        nonce++;
        if (auto [ok, err_msg] = node->getTransactionManager()->insertTransaction(trx); !ok) {
          res["err"] = err_msg;
          break;
        } else {
          inserted++;
        }
      }
    }
    res["status"] = Json::UInt64(inserted);
  } catch (std::exception &e) {
    res["status"] = e.what();
  }

  return res;
}

Json::Value Test::tps_test(const Json::Value &param1) {
  Json::Value res;
  /*try {
    if (auto node = full_node_.lock()) {
      secret_t sk = secret_t(param1["secret"].asString());
      auto nonce = dev::jsToInt(param1["nonce"].asString());
      val_t value = val_t(param1["value"].asString());
      val_t gas_price = val_t(param1["gas_price"].asString());
      auto gas = dev::jsToInt(param1["gas"].asString());
+}
+}
+catch (std::exception &e) {
  res["status"] = e.what();
+}
+*/
  new std::thread([this]() {
    auto node = full_node_.lock();
    uint64_t trx_count = 200000;
    std::vector<std::shared_ptr<Transaction>> trxs;
    for (uint64_t i = 0; i < trx_count; i++) {
      static std::atomic<uint64_t> nonce = 1000001;
      trxs.emplace_back(std::make_shared<Transaction>(++nonce, 10, 1, 21000, bytes(), node->getSecretKey(),
                                                      addr_t(100000000l + (i % 2000)),
                                                      node->getConfig().genesis.chain_id));
      if (i % 10000 == 0) {
        std::cout << "Transactions created: " << i << std::endl;
      }
    }

    auto inexe = node->getDB()->getNumTransactionExecuted();
    auto now = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < trx_count; i++) {
      // addr_t to(100000 + i);
      // auto result = trx_client.coinTransfer(/*KeyPair::create().address()*/ addr_t(100000000l + (i % 50)), 10, {},
      // false); EXPECT_EQ(result.stage, TransactionClient::TransactionStage::inserted);

      /*std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> txs1{
          {trxs[i], TransactionStatus::Verified}};
      node->getTransactionManager()->insertValidatedTransactions(std::move(txs1));*/
      if (i < trx_count - 1) {
        auto r = node->getTransactionManager()->insertTransaction(trxs[i]);
        if (!r.first) {
          std::cout << "ERROR INSERT: " << r.second << std::endl;
        }
        if (i % 5 == 0) thisThreadSleepForMilliSeconds(1);
      } else {
        i--;
        thisThreadSleepForMilliSeconds(1000);
      }

      if (i % 10000 == 0 || i == trx_count - 2) {
        auto elapsed_time_in_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count();

        if (elapsed_time_in_ms) {
          std::cout << "Node: "
                    << ". Transactions inserted: " << i
                    << " N1 Transactions executed: " << node->getDB()->getNumTransactionExecuted() - inexe
                    << " Transactions in DAG: " << node->getDB()->getNumTransactionInDag()
                    << " Transactions executed per second: "
                    << (node->getDB()->getNumTransactionExecuted() - inexe) * 1000 / elapsed_time_in_ms << std::endl;
        }

        // Stop feeding transactions if too many transactions in pool or unexecuted
        // while (node->getTransactionManager()->getTransactionPoolSize() > 100000 ||
        //       node->getDB()->getNumTransactionInDag() - node->getDB()->getNumTransactionExecuted() > 100000) {
        //  thisThreadSleepForMilliSeconds(1);
        //}
      }
    }
  });

  if (param1.isNull()) return res;
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
      const auto chain_size = node->getPbftChain()->getPbftChainSize();
      const auto dpos_total_votes_opt = node->getPbftManager()->getCurrentDposTotalVotesCount();
      const auto dpos_node_votes_opt = node->getPbftManager()->getCurrentNodeVotesCount();
      const auto two_t_plus_one_opt = node->getVoteManager()->getPbftTwoTPlusOne(chain_size, PbftVoteTypes::cert_vote);

      res["synced"] = !node->getNetwork()->pbft_syncing();
      res["syncing_seconds"] = Json::UInt64(node->getNetwork()->syncTimeSeconds());
      res["peer_count"] = Json::UInt64(node->getNetwork()->getPeerCount());
      res["node_count"] = Json::UInt64(node->getNetwork()->getNodeCount());
      res["blk_executed"] = Json::UInt64(node->getDB()->getNumBlockExecuted());
      res["blk_count"] = Json::UInt64(node->getDB()->getDagBlocksCount());
      res["trx_executed"] = Json::UInt64(node->getDB()->getNumTransactionExecuted());
      res["trx_count"] = Json::UInt64(node->getTransactionManager()->getTransactionCount());
      res["dag_level"] = Json::UInt64(node->getDagManager()->getMaxLevel());
      res["pbft_size"] = Json::UInt64(chain_size);
      res["pbft_sync_period"] = Json::UInt64(node->getPbftManager()->pbftSyncingPeriod());
      res["pbft_round"] = Json::UInt64(node->getPbftManager()->getPbftRound());
      res["dpos_total_votes"] = Json::UInt64(dpos_total_votes_opt.has_value() ? *dpos_total_votes_opt : 0);
      res["dpos_node_votes"] = Json::UInt64(dpos_node_votes_opt ? *dpos_node_votes_opt : 0);
      res["dpos_quorum"] = Json::UInt64(two_t_plus_one_opt ? *two_t_plus_one_opt : 0);
      res["pbft_sync_queue_size"] = Json::UInt64(node->getPbftManager()->periodDataQueueSize());
      res["trx_pool_size"] = Json::UInt64(node->getTransactionManager()->getTransactionPoolSize());
      res["trx_nonfinalized_size"] = Json::UInt64(node->getTransactionManager()->getNonfinalizedTrxSize());
      res["network"] = node->getNetwork()->getStatus();
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }
  return res;
}

Json::Value Test::get_all_nodes() {
  Json::Value res;

  try {
    if (auto full_node = full_node_.lock()) {
      auto nodes = full_node->getNetwork()->getAllNodes();
      res["nodes_count"] = Json::UInt64(nodes.size());
      res["nodes"] = Json::Value(Json::arrayValue);
      for (auto const &n : nodes) {
        Json::Value node;
        node["node_id"] = n.id().toString();
        node["address"] = n.endpoint().address().to_string();
        node["listen_port"] = Json::UInt64(n.endpoint().tcpPort());
        res["nodes"].append(node);
      }
    }
  } catch (std::exception &e) {
    res["status"] = e.what();
  }

  return res;
}

}  // namespace taraxa::net
