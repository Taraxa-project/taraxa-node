#include "Test.h"

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>

#include "common/types.hpp"
#include "dag/dag_manager.hpp"
#include "network/network.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote_manager/vote_manager.hpp"

using namespace std;
using namespace dev;
using namespace ::taraxa::final_chain;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

Json::Value Test::get_sortition_change(const Json::Value &param1) {
  Json::Value res;
  if (auto node = app_.lock()) {
    uint64_t period = param1["period"].asUInt64();
    auto params_change = node->getDB()->getParamsChangeForPeriod(period);
    res["interval_efficiency"] = params_change->interval_efficiency;
    res["period"] = params_change->period;
    res["threshold_upper"] = params_change->vrf_params.threshold_upper;
    res["kThresholdUpperMinValue"] = params_change->vrf_params.kThresholdUpperMinValue;
  }
  return res;
}

Json::Value Test::send_coin_transaction(const Json::Value &param1) {
  Json::Value res;
  if (auto node = app_.lock()) {
    secret_t sk = secret_t(param1["secret"].asString());
    uint64_t nonce = 0;
    if (!param1["nonce"]) {
      auto acc = node->getFinalChain()->getAccount(toAddress(sk));
      nonce = acc->nonce.convert_to<uint64_t>() + 1;
    } else {
      nonce = dev::jsToInt(param1["nonce"].asString());
    }
    val_t value = val_t(param1["value"].asString());
    val_t gas_price = val_t(param1["gasPrice"].asString());
    auto gas = dev::jsToInt(param1["gas"].asString());
    addr_t receiver = addr_t(param1["receiver"].asString());
    auto trx = std::make_shared<Transaction>(nonce, value, gas_price, gas, bytes(), sk, receiver, kChainId);
    if (auto [ok, err_msg] = node->getTransactionManager()->insertTransaction(trx); !ok) {
      res["status"] = err_msg;
    } else {
      res = toHex(trx->rlp());
    }
  }
  return res;
}

Json::Value Test::send_coin_transactions(const Json::Value &param1) {
  Json::Value res;
  uint32_t inserted = 0;
  if (auto node = app_.lock()) {
    secret_t sk = secret_t(param1["secret"].asString());
    auto nonce = param1["nonce"].asUInt64();
    val_t value = val_t(param1["value"].asString());
    val_t gas_price = val_t(param1["gasPrice"].asString());
    auto gas = dev::jsToInt(param1["gas"].asString());
    auto transactions_count = param1["transaction_count"].asUInt64();
    std::vector<addr_t> receivers;
    std::transform(param1["receiver"].begin(), param1["receiver"].end(), std::back_inserter(receivers),
                   [](const auto rec) { return addr_t(rec.asString()); });
    for (uint32_t i = 0; i < transactions_count; i++) {
      auto trx = std::make_shared<Transaction>(nonce, value, gas_price, gas, bytes(), sk,
                                               receivers[i % receivers.size()], kChainId);
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

  return res;
}

Json::Value Test::tps_test(const Json::Value &param1) {
  Json::Value res;
  new std::thread([this, param1]() {
    auto node = app_.lock();

    // Test parameters:
    // Total number of transactions:
    uint64_t trx_count = param1["tx_count"].asUInt64();  // [100000]
    // Transactions per second to send transactions
    uint32_t tps = param1["tps"].asUInt64();  // [5000]
    // Maximum size of transaction pool, if it goes over this number sending transactions are paused
    uint32_t max_pool_size = 80000;

    // To initially found the 10 account below use:
    /*curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret":
      "","value": "22000000000","gas_price": "1","gas": "21000","nonce":
      "2","receiver":"0x3d8432060ea8216aa5d9f22991c1622a6fc68349","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "3","receiver":"0x80946cb9cf31d54f02e242f2ddec5155ff650bca","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "4","receiver":"0x07d784cab6a4d6712d38b8526dd0454baf1766ed","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "5","receiver":"0x0fcd9ef355c4ac9e9ed0aadf0cb1d80615f54691","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "6","receiver":"0xc4a41d5b7eb9bae765f3df6f68b70d378074dfcb","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "7","receiver":"0x0e183139741de724a9c90d0341cf816aa85b5798","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "8","receiver":"0x068b9c19ef242c51fe41949565517a90b5e6a0ff","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "9","receiver":"0xc8142e6bd6200425b401cf1ff58bf5d0d08525bd","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "10","receiver":"0x033df3ebc6de21f46b60373a8ad047c86491b84d","chain_id":"34B"}]}' 0.0.0.0:7777 curl -m 10 -s -d
      '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction", "params": [{"secret": "","value":
      "22000000000","gas_price": "1","gas": "21000","nonce":
      "11","receiver":"0xde68e530adc067b82abb683e4fa2ead6bd93e0ff","chain_id":"34B"}]}' 0.0.0.0:7777
    */

    std::vector<dev::KeyPair> keys;
    keys.push_back(dev::KeyPair(dev::Secret("35b6f98b34bbb40d2fe3cbf622da87f50dd868264a32bba798948d22f79781fc",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("e993cbf280e99deb7754f3c191447e29fb3e0e023204e64bf1f5aefec7ad3a39",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("3047c6855e4707dc1ff04a40d6df3e3b4441e76daa521273c833a48118352f2b",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("0cbef99d879ba20444a0fdc2961a5eeed28aa9a8c906dbe0cf106624a9a23543",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("45263bcb4ad4aa7e2f4b99dff9dba6ac3e2683bd2c09ef41a9b6bd4b83964dab",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("225d99c169b0804af481c262d01e8cd430101e9cf41df024a8011979050e8c5e",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("740244d38f1504f2ae428d318f3631314a96cf56e5e6a2b5acddabaef9f591cc",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("88d4a3d8e5ab8b4c2f67dc8cc362c66f1a24820f119ab2f5396a588879433133",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("122cf87326b4637c7c519d02d2faaeec2b903f2d93d4b8c420b1b22c835c3648",
                                            dev::Secret::ConstructFromStringType::FromHex)));
    keys.push_back(dev::KeyPair(dev::Secret("7bfb6597df1742d9f47872ed899c58fc742ef78b55c7a88cf7774e1c0be38911",
                                            dev::Secret::ConstructFromStringType::FromHex)));

    trx_nonce_t max_nonce = 0;
    for (const auto &key : keys) {
      const auto acc = node->getFinalChain()->getAccount(key.address());
      if (acc.has_value() && acc->nonce > max_nonce) {
        max_nonce = acc->nonce;
      }
    }

    std::vector<std::shared_ptr<Transaction>> trxs;
    for (uint64_t i = 0; i < trx_count; i++) {
      trxs.emplace_back(std::make_shared<Transaction>(++max_nonce, 1, 1000000000, 21000, bytes(),
                                                      keys[(i % keys.size())].secret(), addr_t(100000000l + (i % 2000)),
                                                      node->getConfig().genesis.chain_id));
      if (i % 10000 == 0 && i > 0) {
        std::cout << "Transactions created: " << i << std::endl;
      }
    }

    auto inexe = node->getDB()->getNumTransactionExecuted();
    auto now = std::chrono::steady_clock::now();
    uint64_t ee = 0;
    for (uint64_t i = 0; i < trx_count; i++) {
      if (i % 100 == 0)
        ee = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - now).count();

      if (i < trx_count - 1) {
        auto res = node->getTransactionManager()->insertTransaction(trxs[i]);
        if (!res.first) std::cout << res.second << std::endl;
        // Sleep if transactions are created faster than tps or if pool size is over max
        while (ee * tps < i || node->getTransactionManager()->getTransactionPoolSize() > max_pool_size) {
          thisThreadSleepForMilliSeconds(1);
          ee = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - now).count();
        }
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
                    << " Transactions executed: " << node->getDB()->getNumTransactionExecuted() - inexe
                    << " Transactions in DAG: " << node->getDB()->getNumTransactionInDag()
                    << " Transactions executed per second: "
                    << (node->getDB()->getNumTransactionExecuted() - inexe) * 1000 / elapsed_time_in_ms << std::endl;
        }
      }
    }
  });

  return res;
}

Json::Value Test::get_account_address() {
  Json::Value res;
  if (auto node = app_.lock()) {
    addr_t addr = node->getAddress();
    res["value"] = addr.toString();
  }
  return res;
}

Json::Value Test::get_peer_count() {
  Json::Value res;
  if (auto node = app_.lock()) {
    auto peer = node->getNetwork()->getPeerCount();
    res["value"] = Json::UInt64(peer);
  }
  return res;
}

Json::Value Test::get_node_status() {
  Json::Value res;
  if (auto node = app_.lock()) {
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
  return res;
}

Json::Value Test::get_all_nodes() {
  Json::Value res;

  if (auto full_node = app_.lock()) {
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
  return res;
}

}  // namespace taraxa::net
