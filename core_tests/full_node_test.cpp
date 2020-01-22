
#include "full_node.hpp"

#include <gtest/gtest.h>
#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>
#include <libethcore/Precompiled.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "core_tests/util.hpp"
#include "core_tests/util/transaction_client.hpp"
#include "create_samples.hpp"
#include "dag.hpp"
#include "net/RpcServer.h"
#include "net/Taraxa.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "replay_protection/replay_protection_service_test.hpp"
#include "sortition.hpp"
#include "static_init.hpp"
#include "string"
#include "top.hpp"
#include "util.hpp"
#include "util/lazy.hpp"
#include "util/wait.hpp"

namespace taraxa {
using namespace core_tests::util;
using samples::sendTrx;
using ::taraxa::util::lazy::Lazy;
using transaction_client::TransactionClient;
using wait::wait;
using wait::WaitOptions;

const unsigned NUM_TRX = 200;
const unsigned SYNC_TIMEOUT = 400;
auto g_secret = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_signed_samples =
    Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_mock_dag0 = Lazy([] { return samples::createMockDag0(); });
auto g_test_account = Lazy([] {
  return samples::createTestAccountTable("core_tests/account_table.txt");
});
uint64_t g_init_bal = TARAXA_COINS_DECIMAL / 5;

const char *input1[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa1.json",
                        "-v",
                        "4",
                        "--destroy_db"};
const char *input1_persist_db[] = {"./build/main", "--conf_taraxa",
                                   "./core_tests/conf/conf_taraxa1.json", "-v",
                                   "4"};
const char *input2[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa2.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input2_persist_db[] = {"./build/main", "--conf_taraxa",
                                   "./core_tests/conf/conf_taraxa2.json", "-v",
                                   "-2"};
const char *input3[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa3.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input4[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa4.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input5[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa5.json",
                        "-v",
                        "-2",
                        "--destroy_db"};

void send_2_nodes_trxs() {
  std::string sendtrx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 500, 
                                      "number": 600, 
                                      "nonce": 0, 
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777)";
  std::string sendtrx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 700, 
                                      "number": 400, 
                                      "nonce": 600 , 
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"}]}' 0.0.0.0:7778)";
  std::cout << "Sending trxs ..." << std::endl;
  std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
  std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });

  t1.join();
  t2.join();
  std::cout << "All trxs sent..." << std::endl;
}

void init_5_nodes_coin() {
  std::string node1to2 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 0,
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);
  std::string node1to3 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 1,
                                        "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);

  std::string node1to4 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 2,
                                        "receiver":"415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);

  std::string node1to5 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 3,
                                        "receiver":"b770f7a99d0b7ad9adf6520be77ca20ee99b0858"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);
  std::cout << "Init 5 node coins ..." << std::endl;

  std::thread t1([node1to2]() { system(node1to2.data()); });
  std::thread t2([node1to3]() { system(node1to3.data()); });
  std::thread t3([node1to4]() { system(node1to4.data()); });
  std::thread t4([node1to5]() { system(node1to5.data()); });
  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

void send_5_nodes_trxs() {
  // Note: after init_5_nodes_coin, boot node nonce start from 4
  std::string sendtrx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 5,
                                      "number": 2000,
                                      "nonce": 4,
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0" }]}' 0.0.0.0:7777 > /dev/null )";
  std::string sendtrx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 7, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e" }]}' 0.0.0.0:7778 > /dev/null)";
  std::string sendtrx3 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f",
                                      "delay": 3, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0" }]}' 0.0.0.0:7779 > /dev/null)";
  std::string sendtrx4 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029",
                                      "delay": 10, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"b770f7a99d0b7ad9adf6520be77ca20ee99b0858" }]}' 0.0.0.0:7780 > /dev/null)";
  std::string sendtrx5 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe",
                                      "delay": 2,
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"d79b2575d932235d87ea2a08387ae489c31aa2c9" }]}' 0.0.0.0:7781 > /dev/null)";
  std::cout << "Sending trxs ..." << std::endl;
  std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
  std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });
  std::thread t3([sendtrx3]() { system(sendtrx3.c_str()); });
  std::thread t4([sendtrx4]() { system(sendtrx4.c_str()); });
  std::thread t5([sendtrx5]() { system(sendtrx5.c_str()); });

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();

  std::cout << "All trxs sent..." << std::endl;
}  // namespace taraxa

void send_dummy_trx() {
  std::string dummy_trx =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": 0,
                                        "gas_price": 1,
                                        "gas": 100000,
                                        "nonce": 2004,
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)";

  std::cout << "Send dummy transaction ..." << std::endl;
  system(dummy_trx.c_str());
}
struct FullNodeTest : core_tests::util::DBUsingTest<> {};

TEST_F(FullNodeTest, db_test) {
  DbStorage db("/tmp/testtaraxadb", blk_hash_t(1), true);
  DagBlock blk1(blk_hash_t(1), 1, {}, {trx_hash_t(1), trx_hash_t(2)},
                sig_t(777), blk_hash_t(0xB1), addr_t(999));
  DagBlock blk2(blk_hash_t(1), 1, {}, {trx_hash_t(3), trx_hash_t(4)},
                sig_t(777), blk_hash_t(0xB2), addr_t(999));
  DagBlock blk3(blk_hash_t(0xB1), 2, {}, {trx_hash_t(5)}, sig_t(777),
                blk_hash_t(0xB6), addr_t(999));
  db.saveDagBlock(blk1);
  db.saveDagBlock(blk2);
  db.saveDagBlock(blk3);
  EXPECT_EQ(blk1, *db.getDagBlock(blk1.getHash()));
  EXPECT_EQ(blk2, *db.getDagBlock(blk2.getHash()));
  EXPECT_EQ(blk3, *db.getDagBlock(blk3.getHash()));
  EXPECT_EQ(db.getBlocksByLevel(1),
            blk1.getHash().toString() + "," + blk2.getHash().toString());
  EXPECT_EQ(db.getBlocksByLevel(2), blk3.getHash().toString());

  db.saveTransaction(g_trx_signed_samples[0]);
  db.saveTransaction(g_trx_signed_samples[1]);
  auto batch = db.createWriteBatch();
  db.addTransactionToBatch(g_trx_signed_samples[2], batch);
  db.addTransactionToBatch(g_trx_signed_samples[3], batch);
  db.commitWriteBatch(batch);
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[0].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[1].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[2].getHash()));
  EXPECT_TRUE(db.transactionInDb(g_trx_signed_samples[3].getHash()));
  EXPECT_EQ(g_trx_signed_samples[0],
            *db.getTransaction(g_trx_signed_samples[0].getHash()));
  EXPECT_EQ(g_trx_signed_samples[1],
            *db.getTransaction(g_trx_signed_samples[1].getHash()));
  EXPECT_EQ(g_trx_signed_samples[2],
            *db.getTransaction(g_trx_signed_samples[2].getHash()));
  EXPECT_EQ(g_trx_signed_samples[3],
            *db.getTransaction(g_trx_signed_samples[3].getHash()));

  db.saveTransactionToBlock(g_trx_signed_samples[0].getHash(), blk1.getHash());
  db.saveTransactionToBlock(g_trx_signed_samples[1].getHash(), blk1.getHash());
  db.saveTransactionToBlock(g_trx_signed_samples[2].getHash(), blk2.getHash());
  EXPECT_TRUE(db.transactionToBlockInDb(g_trx_signed_samples[0].getHash()));
  EXPECT_TRUE(db.transactionToBlockInDb(g_trx_signed_samples[1].getHash()));
  EXPECT_TRUE(db.transactionToBlockInDb(g_trx_signed_samples[2].getHash()));
  EXPECT_EQ(*db.getTransactionToBlock(g_trx_signed_samples[0].getHash()),
            blk1.getHash());
  EXPECT_EQ(*db.getTransactionToBlock(g_trx_signed_samples[1].getHash()),
            blk1.getHash());
  EXPECT_EQ(*db.getTransactionToBlock(g_trx_signed_samples[2].getHash()),
            blk2.getHash());

  PbftBlock pbft_block1(blk_hash_t(1), 2);
  PbftBlock pbft_block2(blk_hash_t(2), 3);
  db.savePbftBlock(pbft_block1);
  db.savePbftBlock(pbft_block2);
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block1.getBlockHash()));
  EXPECT_TRUE(db.pbftBlockInDb(pbft_block2.getBlockHash()));
  EXPECT_EQ(db.getPbftBlock(pbft_block1.getBlockHash())->rlp(),
            pbft_block1.rlp());
  EXPECT_EQ(db.getPbftBlock(pbft_block2.getBlockHash())->rlp(),
            pbft_block2.rlp());

  db.savePbftBlockOrder("1", blk_hash_t(1));
  db.savePbftBlockOrder("2", blk_hash_t(2));
  EXPECT_EQ(*db.getPbftBlockOrder("1"), blk_hash_t(1));
  EXPECT_EQ(*db.getPbftBlockOrder("2"), blk_hash_t(2));

  db.saveDagBlockOrder("1", blk_hash_t(1));
  db.saveDagBlockOrder("3", blk_hash_t(2));
  EXPECT_EQ(*db.getDagBlockOrder("1"), blk_hash_t(1));
  EXPECT_EQ(*db.getDagBlockOrder("3"), blk_hash_t(2));

  db.saveDagBlockHeight(blk_hash_t(1), "5");
  db.saveDagBlockHeight(blk_hash_t(2), "6");
  EXPECT_EQ(db.getDagBlockHeight(blk_hash_t(1)), "5");
  EXPECT_EQ(db.getDagBlockHeight(blk_hash_t(2)), "6");

  db.saveStatusField(StatusDbField::TrxCount, 5);
  db.saveStatusField(StatusDbField::ExecutedBlkCount, 6);
  EXPECT_EQ(db.getStatusField(StatusDbField::TrxCount), 5);
  EXPECT_EQ(db.getStatusField(StatusDbField::ExecutedBlkCount), 6);

  bytes b1;
  b1.push_back(100);
  bytes b2;
  b1.push_back(100);
  b2.push_back(101);
  db.saveVote(blk_hash_t(1), b1);
  db.saveVote(blk_hash_t(2), b2);
  EXPECT_EQ(db.getVote(blk_hash_t(1)), b1);
  EXPECT_EQ(db.getVote(blk_hash_t(2)), b2);

  db.savePeriodScheduleBlock(1, blk_hash_t(1));
  db.savePeriodScheduleBlock(3, blk_hash_t(2));
  EXPECT_EQ(*db.getPeriodScheduleBlock(1), blk_hash_t(1));
  EXPECT_EQ(*db.getPeriodScheduleBlock(3), blk_hash_t(2));

  db.saveDagBlockPeriod(blk_hash_t(1), 1);
  db.saveDagBlockPeriod(blk_hash_t(2), 2);
  batch = db.createWriteBatch();
  db.addDagBlockPeriodToBatch(blk_hash_t(3), 3, batch);
  db.addDagBlockPeriodToBatch(blk_hash_t(4), 4, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(1, *db.getDagBlockPeriod(blk_hash_t(1)));
  EXPECT_EQ(2, *db.getDagBlockPeriod(blk_hash_t(2)));
  EXPECT_EQ(3, *db.getDagBlockPeriod(blk_hash_t(3)));
  EXPECT_EQ(4, *db.getDagBlockPeriod(blk_hash_t(4)));

  batch = db.createWriteBatch();
  PbftSortitionAccount account1(
      addr_t("123", addr_t::FromHex, addr_t::AlignLeft), 100, 1,
      PbftSortitionAccountStatus::new_change);
  PbftSortitionAccount account2(
      addr_t("345", addr_t::FromHex, addr_t::AlignLeft), 100, 2,
      PbftSortitionAccountStatus::new_change);
  db.addSortitionAccountToBatch(account1.address, account1, batch);
  db.addSortitionAccountToBatch(account2.address, account2, batch);
  db.commitWriteBatch(batch);
  EXPECT_EQ(account1.getJsonStr(),
            db.getSortitionAccount(account1.address).getJsonStr());
  EXPECT_EQ(account2.getJsonStr(),
            db.getSortitionAccount(account2.address).getJsonStr());
}

// fixme: flaky
TEST_F(FullNodeTest, sync_five_nodes) {
  using namespace std;
  Top top1(5, input1);
  std::cout << "Top1 created ..." << std::endl;
  Top top2(5, input2);
  std::cout << "Top2 created ..." << std::endl;
  Top top3(5, input3);
  std::cout << "Top3 created ..." << std::endl;
  Top top4(5, input4);
  std::cout << "Top4 created ..." << std::endl;
  Top top5(5, input5);
  std::cout << "Top5 created ..." << std::endl;

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();
  auto node4 = top4.getNode();
  auto node5 = top5.getNode();

  taraxa::thisThreadSleepForMilliSeconds(500);

  ASSERT_GT(node1->getPeerCount(), 0);
  ASSERT_GT(node2->getPeerCount(), 0);
  ASSERT_GT(node3->getPeerCount(), 0);
  ASSERT_GT(node4->getPeerCount(), 0);
  ASSERT_GT(node5->getPeerCount(), 0);
  std::vector<std::shared_ptr<FullNode>> nodes;
  nodes.emplace_back(node1);
  nodes.emplace_back(node2);
  nodes.emplace_back(node3);
  nodes.emplace_back(node4);
  nodes.emplace_back(node5);

  EXPECT_EQ(node1->getDagBlockMaxHeight(), 1);  // genesis block

  class context {
    decltype(nodes) const &nodes_;
    vector<TransactionClient> trx_clients;
    uint64_t issued_trx_count = 0;
    unordered_map<addr_t, val_t> expected_balances;
    shared_mutex m;

   public:
    context(decltype(nodes_) nodes) : nodes_(nodes) {
      for (auto &[addr, acc] : nodes[0]->getConfig().chain.eth.genesisState) {
        expected_balances[addr] = acc.balance();
      }
      for (uint i(0), cnt(nodes_.size()); i < cnt; ++i) {
        auto const &backend = nodes_[(i + 1) % cnt];  // shuffle a bit
        trx_clients.emplace_back(nodes_[i]->getSecretKey(), backend);
      }
    }

    auto getIssuedTrxCount() {
      shared_lock l(m);
      return issued_trx_count;
    }

    void coin_transfer(int sender_node_i, addr_t const &to,
                       val_t const &amount) {
      {
        unique_lock l(m);
        ++issued_trx_count;
        expected_balances[to] += amount;
        expected_balances[nodes_[sender_node_i]->getAddress()] -= amount;
        std::cout << nodes_[sender_node_i]->getAddress() << " send to " << to
                  << " value " << amount << std::endl;
      }
      auto result = trx_clients[sender_node_i].coinTransfer(to, amount);
      EXPECT_EQ(result.stage, TransactionClient::TransactionStage::executed);
    }

    void assert_balances_synced() {
      shared_lock l(m);
      for (auto &[addr, val] : expected_balances) {
        for (auto &node : nodes_) {
          ASSERT_EQ(val, node->getBalance(addr).first);
        }
      }
    }

  } context(nodes);

  // transfer some coins to your friends ...
  auto init_bal = TARAXA_COINS_DECIMAL / nodes.size();
  for (auto i(1); i < nodes.size(); ++i) {
    context.coin_transfer(0, nodes[i]->getAddress(), init_bal);
  }

  std::cout << "Initial coin transfers from node 0 issued ... " << std::endl;

  {
    vector<thread> threads;
    for (auto i(0); i < nodes.size(); ++i) {
      auto to = i < nodes.size() - 1
                    ? nodes[i + 1]->getAddress()
                    : addr_t("d79b2575d932235d87ea2a08387ae489c31aa2c9");
      threads.emplace_back([i, to, &context] {
        for (auto _(0); _ < 10; ++_) {
          context.coin_transfer(i, to, 100);
        }
      });
    }
    for (auto &t : threads) {
      t.join();
    }
  }
  std::cout << "Issued transatnion count " << context.getIssuedTrxCount();

  auto TIMEOUT = SYNC_TIMEOUT;
  for (auto i = 0; i < TIMEOUT; i++) {
    auto num_vertices1 = node1->getNumVerticesInDag();
    auto num_vertices2 = node2->getNumVerticesInDag();
    auto num_vertices3 = node3->getNumVerticesInDag();
    auto num_vertices4 = node4->getNumVerticesInDag();
    auto num_vertices5 = node5->getNumVerticesInDag();

    auto num_trx1 = node1->getTransactionStatusCount();
    auto num_trx2 = node2->getTransactionStatusCount();
    auto num_trx3 = node3->getTransactionStatusCount();
    auto num_trx4 = node4->getTransactionStatusCount();
    auto num_trx5 = node5->getTransactionStatusCount();

    auto issued_trx_count = context.getIssuedTrxCount();

    if (num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 &&
        num_vertices3 == num_vertices4 && num_vertices4 == num_vertices5 &&
        num_trx1 == issued_trx_count && num_trx2 == issued_trx_count &&
        num_trx3 == issued_trx_count && num_trx4 == issued_trx_count &&
        num_trx5 == issued_trx_count) {
      break;
    }

    if (i % 10 == 0) {
      std::cout << "Still waiting for DAG vertices and transaction gossiping "
                   "to sync ..."
                << std::endl;
      std::cout << " Node 1: Dag size = " << num_vertices1.first
                << " Trx count = " << num_trx1 << std::endl
                << " Node 2: Dag size = " << num_vertices2.first
                << " Trx count = " << num_trx2 << std::endl
                << " Node 3: Dag size = " << num_vertices3.first
                << " Trx count = " << num_trx3 << std::endl
                << " Node 4: Dag size = " << num_vertices4.first
                << " Trx count = " << num_trx4 << std::endl
                << " Node 5: Dag size = " << num_vertices5.first
                << " Trx count = " << num_trx5
                << " Issued transaction count = " << issued_trx_count
                << std::endl;
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  EXPECT_GT(node1->getNumProposedBlocks(), 2);
  EXPECT_GT(node2->getNumProposedBlocks(), 2);
  EXPECT_GT(node3->getNumProposedBlocks(), 2);
  EXPECT_GT(node4->getNumProposedBlocks(), 2);
  EXPECT_GT(node5->getNumProposedBlocks(), 2);

  ASSERT_EQ(node1->getTransactionStatusCount(), context.getIssuedTrxCount());
  ASSERT_EQ(node2->getTransactionStatusCount(), context.getIssuedTrxCount());
  ASSERT_EQ(node3->getTransactionStatusCount(), context.getIssuedTrxCount());
  ASSERT_EQ(node4->getTransactionStatusCount(), context.getIssuedTrxCount());
  ASSERT_EQ(node5->getTransactionStatusCount(), context.getIssuedTrxCount());

  // send dummy trx to make sure all DAG blocks are ordered
  // NOTE: have to wait longer than block proposer time + transaction
  // propogation time to ensure
  //       all transacations have already been packed into other blocks and that
  //       this new transaction will get packed into a unique block that will
  //       reference all outstanding tips
  std::cout << "Sleep 2 seconds before sending dummy transaction ... "
            << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);
  std::cout << "Send dummy transaction ... " << std::endl;
  context.coin_transfer(0, addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"),
                        0);

  auto issued_trx_count = context.getIssuedTrxCount();

  std::cout << "Wait 2 seconds before checking all nodes have seen a new DAG "
               "block (containing dummy transaction) ... "
            << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  EXPECT_EQ(num_vertices4, num_vertices5);

  ASSERT_EQ(node1->getTransactionStatusCount(), issued_trx_count);
  ASSERT_EQ(node2->getTransactionStatusCount(), issued_trx_count);
  ASSERT_EQ(node3->getTransactionStatusCount(), issued_trx_count);
  ASSERT_EQ(node4->getTransactionStatusCount(), issued_trx_count);
  ASSERT_EQ(node5->getTransactionStatusCount(), issued_trx_count);

  std::cout << "All transactions received ..." << std::endl;

  TIMEOUT = SYNC_TIMEOUT * 10;
  for (auto i = 0; i < TIMEOUT; i++) {
    auto trx_executed1 = node1->getNumTransactionExecuted();
    auto trx_executed2 = node2->getNumTransactionExecuted();
    auto trx_executed3 = node3->getNumTransactionExecuted();
    auto trx_executed4 = node4->getNumTransactionExecuted();
    auto trx_executed5 = node5->getNumTransactionExecuted();
    // unique trxs in DAG block
    auto trx_packed1 = node1->getPackedTrxs().size();
    auto trx_packed2 = node2->getPackedTrxs().size();
    auto trx_packed3 = node3->getPackedTrxs().size();
    auto trx_packed4 = node4->getPackedTrxs().size();
    auto trx_packed5 = node5->getPackedTrxs().size();

    if (trx_packed1 < trx_executed1) {
      std::cout << "Warning! " << trx_packed1
                << " packed transactions is less than " << trx_executed1
                << " executed transactions in node 1" << std::endl;
    }

    if (trx_executed1 == issued_trx_count &&
        trx_executed2 == issued_trx_count &&
        trx_executed3 == issued_trx_count &&
        trx_executed4 == issued_trx_count &&
        trx_executed5 == issued_trx_count) {
      if (trx_packed1 == issued_trx_count && trx_packed2 == issued_trx_count &&
          trx_packed3 == issued_trx_count && trx_packed4 == issued_trx_count &&
          trx_packed5 == issued_trx_count) {
        break;
      } else {
        std::cout << "Warning! See all nodes with " << issued_trx_count
                  << " executed transactions, "
                     "but not all nodes yet see"
                  << issued_trx_count << " packed transactions!!!";
      }
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
    if (i % 100 == 0) {
      if (trx_executed1 != issued_trx_count) {
        std::cout << " Node 1: executed blk= " << node1->getNumBlockExecuted()
                  << " Dag size: " << node1->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed1 << "/"
                  << issued_trx_count << std::endl;
      }
      if (trx_executed2 != issued_trx_count) {
        std::cout << " Node 2: executed blk= " << node2->getNumBlockExecuted()
                  << " Dag size: " << node2->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed2 << "/"
                  << issued_trx_count << std::endl;
      }
      if (trx_executed3 != issued_trx_count) {
        std::cout << " Node 3: executed blk= " << node3->getNumBlockExecuted()
                  << " Dag size: " << node3->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed3 << "/"
                  << issued_trx_count << std::endl;
      }
      if (trx_executed4 != issued_trx_count) {
        std::cout << " Node 4: executed blk= " << node4->getNumBlockExecuted()
                  << " Dag size: " << node4->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed4 << "/"
                  << issued_trx_count << std::endl;
      }
      if (trx_executed5 != issued_trx_count) {
        std::cout << " Node 5: executed blk= " << node5->getNumBlockExecuted()
                  << " Dag size: " << node5->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed5 << "/"
                  << issued_trx_count << std::endl;
      }
    }
    if (i == TIMEOUT - 1) {
      // last wait
      std::cout << "Warning! Syncing maybe failed ..." << std::endl;
    }
  }

  auto k = 0;
  for (auto const &node : nodes) {
    k++;
    auto vertices_diff =
        node->getNumVerticesInDag().first - 1 - node->getNumBlockExecuted();
    if (vertices_diff >= nodes.size()                             //
        || vertices_diff < 0                                      //
        || node->getNumTransactionExecuted() != issued_trx_count  //
        || node->getPackedTrxs().size() != issued_trx_count) {
      std::cout << "Node " << k
                << " :Number of trx packed = " << node->getPackedTrxs().size()
                << std::endl;
      std::cout << "Node " << k << " :Number of trx executed = "
                << node->getNumTransactionExecuted() << std::endl;

      std::cout << "Node " << k << " :Number of block executed = "
                << node->getNumBlockExecuted() << std::endl;
      auto num_vertices = node->getNumVerticesInDag();
      std::cout << "Node " << k
                << " :Number of vertices in Dag = " << num_vertices.first
                << " , " << num_vertices.second << std::endl;
      auto dags = node->getLinearizedDagBlocks();
      for (auto i(0); i < dags.size(); ++i) {
        auto d = dags[i];
        std::cout << i << " " << d
                  << " trx: " << node1->getDagBlock(d)->getTrxs().size()
                  << std::endl;
      }
      std::string filename = "debug_dag_" + std::to_string(k);
      node->drawGraph(filename);
    }
  }

  k = 0;
  for (auto const &node : nodes) {
    k++;

    EXPECT_EQ(node->getNumTransactionExecuted(), issued_trx_count)
        << " \nNum executed in node " << k << " node " << node
        << " is : " << node->getNumTransactionExecuted()
        << " \nNum linearized blks: " << node->getLinearizedDagBlocks().size()
        << " \nNum executed blks: " << node->getNumBlockExecuted()
        << " \nNum vertices in DAG: " << node->getNumVerticesInDag().first
        << " " << node->getNumVerticesInDag().second << "\n";

    auto vertices_diff =
        node->getNumVerticesInDag().first - 1 - node->getNumBlockExecuted();

    // diff should be larger than 0 but smaller than number of nodes
    // genesis block won't be executed
    EXPECT_LT(vertices_diff, nodes.size())
        << "Failed in node " << k << "node " << node
        << " Number of vertices: " << node->getNumVerticesInDag().first
        << " Number of executed blks: " << node->getNumBlockExecuted()
        << std::endl;
    EXPECT_GE(vertices_diff, 0)
        << "Failed in node " << k << "node " << node
        << " Number of vertices: " << node->getNumVerticesInDag().first
        << " Number of executed blks: " << node->getNumBlockExecuted()
        << std::endl;
    EXPECT_EQ(node->getPackedTrxs().size(), issued_trx_count);
  }

  auto dags = node1->getLinearizedDagBlocks();
  for (auto i(0); i < dags.size(); ++i) {
    auto d = dags[i];
    for (auto const &t : node1->getDagBlock(d)->getTrxs()) {
      auto blk = node1->getDagBlockFromTransaction(t);
      EXPECT_FALSE(blk->isZero());
    }
  }

  context.assert_balances_synced();
}

TEST_F(FullNodeTest, insert_anchor_and_compute_order) {
  auto node(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  node->start(true);  // boot node

  g_mock_dag0 = samples::createMockDag0(
      node->getConfig().chain.dag_genesis_block.getHash().toString());

  auto num_blks = g_mock_dag0->size();
  for (int i = 1; i <= 9; i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);
  std::string pivot;
  std::vector<std::string> tips;

  // -------- first period ----------

  node->getLatestPivotAndTips(pivot, tips);
  uint64_t period;
  std::shared_ptr<vec_blk_t> order;
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 1);
  EXPECT_EQ(order->size(), 6);

  if (order->size() == 6) {
    EXPECT_EQ((*order)[0], blk_hash_t(3));
    EXPECT_EQ((*order)[1], blk_hash_t(6));
    EXPECT_EQ((*order)[2], blk_hash_t(2));
    EXPECT_EQ((*order)[3], blk_hash_t(1));
    EXPECT_EQ((*order)[4], blk_hash_t(5));
    EXPECT_EQ((*order)[5], blk_hash_t(7));
  }
  auto num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 6);
  // -------- second period ----------

  for (int i = 10; i <= 16; i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 2);
  if (order->size() == 7) {
    EXPECT_EQ((*order)[0], blk_hash_t(11));
    EXPECT_EQ((*order)[1], blk_hash_t(10));
    EXPECT_EQ((*order)[2], blk_hash_t(13));
    EXPECT_EQ((*order)[3], blk_hash_t(9));
    EXPECT_EQ((*order)[4], blk_hash_t(12));
    EXPECT_EQ((*order)[5], blk_hash_t(14));
    EXPECT_EQ((*order)[6], blk_hash_t(15));
  }
  num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 7);

  // -------- third period ----------

  for (int i = 17; i < g_mock_dag0->size(); i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 3);
  if (order->size() == 5) {
    EXPECT_EQ((*order)[0], blk_hash_t(17));
    EXPECT_EQ((*order)[1], blk_hash_t(16));
    EXPECT_EQ((*order)[2], blk_hash_t(8));
    EXPECT_EQ((*order)[3], blk_hash_t(18));
    EXPECT_EQ((*order)[4], blk_hash_t(19));
  }
  num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 5);
}

TEST_F(FullNodeTest, destroy_db) {
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     true));  // destroy DB
    node->start(false);
    auto db = node->getDB();
    db->saveTransaction(g_trx_signed_samples[0]);
    // Verify trx saved in db
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0].getHash()));
  }
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     false));  // destroy DB
    node->start(false);
    auto db = node->getDB();
    // Verify trx saved in db after restart with destroy_db false
    EXPECT_TRUE(db->getTransaction(g_trx_signed_samples[0].getHash()));
  }
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     true));  // destroy DB
    node->start(false);
    auto db = node->getDB();
    // Verify trx not in db after restart with destroy_db true
    EXPECT_TRUE(!db->getTransaction(g_trx_signed_samples[0].getHash()));
  }
}

TEST_F(FullNodeTest, destroy_node) {
  std::weak_ptr<FullNode> weak_node;
  std::weak_ptr<Network> weak_network;
  std::weak_ptr<PbftManager> weak_pbft_manager;
  std::weak_ptr<PbftChain> weak_pbft_chain;
  std::weak_ptr<TransactionManager> weak_transaction_manager;
  std::weak_ptr<VoteManager> weak_vote_manager;
  std::weak_ptr<DbStorage> weak_db;
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     true));  // destroy DB
    node->start(false);
    weak_node = node->getShared();
    weak_network = node->getNetwork();
    weak_pbft_manager = node->getPbftManager();
    weak_pbft_chain = node->getPbftChain();
    weak_transaction_manager = node->getTransactionManager();
    weak_vote_manager = node->getVoteManager();
    weak_db = node->getDB();
  }
  // Once node is deleted, verify all objects are deleted
  ASSERT_EQ(weak_node.lock(), nullptr);
  ASSERT_EQ(weak_network.lock(), nullptr);
  ASSERT_EQ(weak_pbft_manager.lock(), nullptr);
  ASSERT_EQ(weak_pbft_chain.lock(), nullptr);
  ASSERT_EQ(weak_transaction_manager.lock(), nullptr);
  ASSERT_EQ(weak_vote_manager.lock(), nullptr);
  ASSERT_EQ(weak_db.lock(), nullptr);

  {
    {
      Top top1(6, input1);
      auto node = top1.getNode();
      weak_node = node->getShared();
      weak_network = node->getNetwork();
      weak_pbft_manager = node->getPbftManager();
      weak_pbft_chain = node->getPbftChain();
      weak_transaction_manager = node->getTransactionManager();
      weak_vote_manager = node->getVoteManager();
      weak_db = node->getDB();
    }
    // Once node is deleted, verify all objects are deleted
    ASSERT_EQ(weak_node.lock(), nullptr);
    ASSERT_EQ(weak_network.lock(), nullptr);
    ASSERT_EQ(weak_pbft_manager.lock(), nullptr);
    ASSERT_EQ(weak_pbft_chain.lock(), nullptr);
    ASSERT_EQ(weak_transaction_manager.lock(), nullptr);
    ASSERT_EQ(weak_vote_manager.lock(), nullptr);
    ASSERT_EQ(weak_db.lock(), nullptr);
  }
}

TEST_F(FullNodeTest, reconstruct_dag) {
  unsigned long vertices1 = 0;
  unsigned long vertices2 = 0;
  unsigned long vertices3 = 0;
  unsigned long vertices4 = 0;

  auto num_blks = g_mock_dag0->size();
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     true));  // destroy DB

    g_mock_dag0 = samples::createMockDag0(
        conf.chain.dag_genesis_block.getHash().toString());

    node->start(false);
    taraxa::thisThreadSleepForMilliSeconds(500);

    for (int i = 1; i < num_blks; i++) {
      node->insertBlock(g_mock_dag0[i]);
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node->getNumVerticesInDag().first;
    EXPECT_EQ(vertices1, num_blks);
  }
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     false));  // no destroy DB

    node->start(false);
    taraxa::thisThreadSleepForMilliSeconds(500);

    vertices2 = node->getNumVerticesInDag().first;
    EXPECT_EQ(vertices2, num_blks);
  }
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     false));  // no destroy DB
    node->start(false);
    // TODO: pbft does not support node stop yet, to be fixed ...
    node->getPbftManager()->stop();
    for (int i = 1; i < num_blks; i++) {
      node->insertBlock(g_mock_dag0[i]);
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);
    vertices3 = node->getNumVerticesInDag().first;
  }
  {
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(taraxa::FullNode::make(conf,
                                     false));  // no destroy DB
    node->start(false);
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices4 = node->getNumVerticesInDag().first;
  }
  EXPECT_EQ(vertices1, vertices2);
  EXPECT_EQ(vertices2, vertices3);
  EXPECT_EQ(vertices3, vertices4);
}

TEST_F(FullNodeTest, sync_two_nodes1) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(6, input2);
  std::cout << "Top2 created ..." << std::endl;

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();

  EXPECT_GT(node1->getPeerCount(), 0);
  EXPECT_GT(node2->getPeerCount(), 0);

  // send 1000 trxs
  try {
    send_2_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto num_trx1 = node1->getTransactionStatusCount();
  auto num_trx2 = node2->getTransactionStatusCount();
  // add more delay if sync is not done
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_trx1 == 1000 && num_trx2 == 1000) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_trx1 = node1->getTransactionStatusCount();
    num_trx2 = node2->getTransactionStatusCount();
  }
  EXPECT_EQ(node1->getTransactionStatusCount(), 1000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 1000);

  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_vertices1.first > 3 && num_vertices2.first > 3 &&
        num_vertices1 == num_vertices2)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GE(num_vertices1.first, 3);
  EXPECT_GE(num_vertices2.second, 3);
  EXPECT_EQ(num_vertices1, num_vertices2);
}

TEST_F(FullNodeTest, persist_counter) {
  unsigned long num_exe_trx1 = 0, num_exe_trx2 = 0, num_exe_blk1 = 0,
                num_exe_blk2 = 0, num_trx1 = 0, num_trx2 = 0;
  {
    Top top1(6, input1);
    std::cout << "Top1 created ..." << std::endl;

    Top top2(6, input2);
    std::cout << "Top2 created ..." << std::endl;

    auto node1 = top1.getNode();
    auto node2 = top2.getNode();

    EXPECT_GT(node1->getPeerCount(), 0);
    EXPECT_GT(node2->getPeerCount(), 0);

    // send 1000 trxs
    try {
      send_2_nodes_trxs();
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    num_trx1 = node1->getTransactionStatusCount();
    num_trx2 = node2->getTransactionStatusCount();
    // add more delay if sync is not done
    for (auto i = 0; i < SYNC_TIMEOUT; i++) {
      if (num_trx1 == 1000 && num_trx2 == 1000) break;
      taraxa::thisThreadSleepForMilliSeconds(500);
      num_trx1 = node1->getTransactionStatusCount();
      num_trx2 = node2->getTransactionStatusCount();
    }
    EXPECT_EQ(node1->getTransactionStatusCount(), 1000);
    EXPECT_EQ(node2->getTransactionStatusCount(), 1000);
    std::cout << "All 1000 trxs are received ..." << std::endl;
    // time to make sure all transactions have been packed into block...
    // taraxa::thisThreadSleepForSeconds(10);
    taraxa::thisThreadSleepForMilliSeconds(2000);
    // send dummy trx to make sure all DAGs are ordered
    try {
      send_dummy_trx();
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    num_exe_trx1 = node1->getNumTransactionExecuted();
    num_exe_trx2 = node2->getNumTransactionExecuted();
    // add more delay if sync is not done
    for (auto i = 0; i < SYNC_TIMEOUT; i++) {
      if (num_exe_trx1 == 1001 && num_exe_trx2 == 1001) break;
      taraxa::thisThreadSleepForMilliSeconds(200);
      num_exe_trx1 = node1->getNumTransactionExecuted();
      num_exe_trx2 = node2->getNumTransactionExecuted();
    }

    ASSERT_EQ(num_exe_trx1, 1001);
    ASSERT_EQ(num_exe_trx2, 1001);

    num_exe_blk1 = node1->getNumBlockExecuted();
    num_exe_blk2 = node2->getNumBlockExecuted();

    num_trx1 = node1->getTransactionCount();
    num_trx2 = node2->getTransactionCount();

    EXPECT_GT(num_exe_blk1, 0);
    EXPECT_EQ(num_exe_blk1, num_exe_blk2);
    EXPECT_EQ(num_exe_trx1, num_trx1);
    EXPECT_EQ(num_exe_trx1, num_trx2);
  }
  {
    Top top1(5, input1_persist_db);
    std::cout << "Top1 created ..." << std::endl;

    Top top2(5, input2_persist_db);
    std::cout << "Top2 created ..." << std::endl;

    auto node1 = top1.getNode();
    auto node2 = top2.getNode();
    EXPECT_EQ(num_exe_trx1, node1->getNumTransactionExecuted());
    EXPECT_EQ(num_exe_trx2, node2->getNumTransactionExecuted());
    EXPECT_EQ(num_exe_blk1, node1->getNumBlockExecuted());
    EXPECT_EQ(num_exe_blk2, node2->getNumBlockExecuted());
    EXPECT_EQ(num_trx1, node1->getTransactionCount());
    EXPECT_EQ(num_trx2, node2->getTransactionCount());
  }
}

TEST_F(FullNodeTest, sync_two_nodes2) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;
  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    sendTrx(1000, 7777);
    std::cout << "1000 trxs sent ..." << std::endl;

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  Top top2(6, input2);
  std::cout << "Top2 created ..." << std::endl;
  // wait for top2 initialize
  auto node1 = top1.getNode();
  auto node2 = top2.getNode();

  EXPECT_GT(node1->getPeerCount(), 0);
  EXPECT_GT(node2->getPeerCount(), 0);

  auto vertices1 = node1->getNumVerticesInDag();
  auto vertices2 = node2->getNumVerticesInDag();
  // let node2 sync node1
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (vertices1 == vertices2 && vertices1.first > 3) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node1->getNumVerticesInDag();
    vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GE(vertices1.first, 3);
  EXPECT_GE(vertices1.second, 3);
  EXPECT_EQ(vertices1, vertices2);
}

TEST_F(FullNodeTest, genesis_balance) {
  addr_t addr1(100);
  val_t bal1(1000);
  addr_t addr2(200);
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  cfg.chain.eth.genesisState[addr1] = dev::eth::Account(0, bal1);
  cfg.chain.eth.calculateStateRoot(true);
  auto node(taraxa::FullNode::make(cfg));
  node->start(true);
  auto res = node->getBalance(addr1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal1);
  res = node->getBalance(addr2);
  EXPECT_FALSE(res.second);
}

// disabled for now, need to create two trx with nonce 0!
TEST_F(FullNodeTest, single_node_run_two_transactions) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;
  auto node1 = top1.getNode();

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868808502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba07659e8c7207a4b2cd96488108fed54c463b1719b438add1159beed04f6660da8a028feb0a3b44bd34e0dd608f82aeeb2cd70d1305653b5dc33678be2ffcfcac997"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf868018502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba05256492dd60623ab5a403ed1b508f845f87f631d2c2e7acd4357cd83ef5b6540a042def7cd4f3c25ce67ee25911740dab47161e096f1dd024badecec58888a890b"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  system(send_raw_trx1.c_str());
  thisThreadSleepForSeconds(10);
  EXPECT_EQ(node1->getTransactionStatusCount(), 1);
  EXPECT_EQ(node1->getNumVerticesInDag().first, 2);
  std::cout << "First trx received ..." << std::endl;

  auto trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, 1);
  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  system(send_raw_trx2.c_str());
  thisThreadSleepForSeconds(10);
  EXPECT_EQ(node1->getTransactionStatusCount(), 2);
  EXPECT_EQ(node1->getNumVerticesInDag().first, 3);

  trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 2) break;
    thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(trx_executed1, 2);
}

TEST_F(FullNodeTest, two_nodes_run_two_transactions) {
  Top top1(6, input1);
  Top top2(6, input2);
  auto node1 = top1.getNode();
  auto node2 = top2.getNode();

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b808502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca04cef8610e05b4476673c369204899da747a9b32b0ad3769814b620281c900408a0130499a83d0b56c184c6ac518f6bbe2f8f946b65bf42b08cfc9b4dfbe2ebfd04"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"eth_sendRawTransaction", "params":
["0xf86b018502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca06644c30a23286d0de8fa107f1bded3a7a214004042b2007b2a9a62c8b313cf79a06cbb522856838b107542d8213286928500b2822584d6c7c6fee3a2c348cade4a"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  system(send_raw_trx1.c_str());
  thisThreadSleepForSeconds(10);
  EXPECT_EQ(node1->getTransactionStatusCount(), 1);
  EXPECT_GE(node1->getNumVerticesInDag().first, 2);
  std::cout << "First trx received ..." << std::endl;

  auto trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(trx_executed1, 1);
  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  system(send_raw_trx2.c_str());
  thisThreadSleepForSeconds(10);
  EXPECT_EQ(node1->getTransactionStatusCount(), 2);
  EXPECT_GE(node1->getNumVerticesInDag().first, 3);

  trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 2) break;
    thisThreadSleepForMilliSeconds(1000);
  }
  EXPECT_EQ(trx_executed1, 2);
}

TEST_F(FullNodeTest, save_network_to_file) {
  {
    FullNodeConfig conf1("./core_tests/conf/conf_taraxa1.json");
    auto node1(taraxa::FullNode::make(conf1));
    FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
    auto node2(taraxa::FullNode::make(conf2));
    FullNodeConfig conf3("./core_tests/conf/conf_taraxa3.json");
    auto node3(taraxa::FullNode::make(conf3));

    node1->start(true);
    node2->start(false);
    node3->start(false);

    for (int i = 0; i < 30; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (2 == node1->getPeerCount() && 2 == node2->getPeerCount() &&
          2 == node3->getPeerCount())
        break;
    }

    ASSERT_EQ(2, node1->getPeerCount());
    ASSERT_EQ(2, node2->getPeerCount());
    ASSERT_EQ(2, node3->getPeerCount());
  }
  {
    FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
    auto node2(taraxa::FullNode::make(conf2));
    FullNodeConfig conf3("./core_tests/conf/conf_taraxa3.json");
    auto node3(taraxa::FullNode::make(conf3));

    node2->start(false);
    node3->start(false);

    for (int i = 0; i < SYNC_TIMEOUT; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (1 == node2->getPeerCount() && 1 == node3->getPeerCount()) break;
    }

    ASSERT_EQ(1, node2->getPeerCount());
    ASSERT_EQ(1, node3->getPeerCount());
  }
}

TEST_F(FullNodeTest, receive_send_transaction) {
  Top top1(5, input1);
  auto node1 = top1.getNode();
  node1->setDebug(true);
  node1->start(true);  // boot node
  try {
    sendTrx(1000, 7777);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  std::cout << "1000 transaction are sent through RPC ..." << std::endl;

  auto num_proposed_blk = node1->getNumProposedBlocks();
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_proposed_blk > 0) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_GT(node1->getNumProposedBlocks(), 0);
}

TEST_F(FullNodeTest, detect_overlap_transactions) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(6, input2);
  std::cout << "Top2 created ..." << std::endl;

  Top top3(6, input3);
  std::cout << "Top3 created ..." << std::endl;

  Top top4(6, input4);
  std::cout << "Top4 created ..." << std::endl;

  Top top5(6, input5);
  std::cout << "Top5 created ..." << std::endl;

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();
  auto node4 = top4.getNode();
  auto node5 = top5.getNode();

  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();
  node3->getPbftManager()->stop();
  node4->getPbftManager()->stop();
  node5->getPbftManager()->stop();

  thisThreadSleepForMilliSeconds(500);

  ASSERT_GT(node1->getPeerCount(), 0);
  ASSERT_GT(node2->getPeerCount(), 0);
  ASSERT_GT(node3->getPeerCount(), 0);
  ASSERT_GT(node4->getPeerCount(), 0);
  ASSERT_GT(node5->getPeerCount(), 0);

  try {
    init_5_nodes_coin();
    thisThreadSleepForSeconds(5);
    send_5_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  // check dags
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    auto num_vertices1 = node1->getNumVerticesInDag();
    auto num_vertices2 = node2->getNumVerticesInDag();
    auto num_vertices3 = node3->getNumVerticesInDag();
    auto num_vertices4 = node4->getNumVerticesInDag();
    auto num_vertices5 = node5->getNumVerticesInDag();

    auto trx_table = node1->getUnsafeTransactionStatusTable();
    int packed_trx = 0;

    if (trx_table.size() == 10004) {
      for (auto const &t : trx_table) {
        if (t.second == TransactionStatus::in_block) {
          packed_trx++;
        }
      }

      if (packed_trx == 10004 && num_vertices1 == num_vertices2 &&
          num_vertices2 == num_vertices3 && num_vertices3 == num_vertices4 &&
          num_vertices4 == num_vertices5 &&
          node1->getTransactionStatusCount() == 10004 &&
          node2->getTransactionStatusCount() == 10004 &&
          node3->getTransactionStatusCount() == 10004 &&
          node4->getTransactionStatusCount() == 10004 &&
          node5->getTransactionStatusCount() == 10004) {
        break;
      }
      if (i % 10 == 0) {
        std::cout << "Wait for vertices syncing ... packed trx " << packed_trx
                  << std::endl;
        std::cout << "Node 1 trx received: "
                  << node1->getTransactionStatusCount()
                  << " Dag size: " << num_vertices1 << std::endl;
        std::cout << "Node 2 trx received: "
                  << node2->getTransactionStatusCount()
                  << " Dag size: " << num_vertices2 << std::endl;
        std::cout << "Node 3 trx received: "
                  << node3->getTransactionStatusCount()
                  << " Dag size: " << num_vertices3 << std::endl;
        std::cout << "Node 4 trx received: "
                  << node4->getTransactionStatusCount()
                  << " Dag size: " << num_vertices4 << std::endl;
        std::cout << "Node 5 trx received: "
                  << node5->getTransactionStatusCount()
                  << " Dag size: " << num_vertices5 << std::endl;
      }
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  EXPECT_GT(node1->getNumProposedBlocks(), 2);
  EXPECT_GT(node2->getNumProposedBlocks(), 2);
  EXPECT_GT(node3->getNumProposedBlocks(), 2);
  EXPECT_GT(node4->getNumProposedBlocks(), 2);
  EXPECT_GT(node5->getNumProposedBlocks(), 2);

  ASSERT_EQ(node1->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node2->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node3->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node4->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node5->getTransactionStatusCount(), 10004);

  // send dummy trx to make sure all DAG blocks are ordered
  // NOTE: have to wait longer than block proposer time + transaction
  // propogation time to ensure
  //       all transacations have already been packed into other blocks and that
  //       this new transaction will get packed into a unique block that will
  //       reference all outstanding tips
  std::cout << "Sleep 2 seconds before sending dummy transaction ... "
            << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  // send dummy trx to make sure all DAGs are ordered
  std::cout << "Send dummy transaction ... " << std::endl;
  try {
    send_dummy_trx();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "Wait 2 seconds before checking all nodes have seen a new DAG "
               "block (containing dummy transaction) ... "
            << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  EXPECT_EQ(num_vertices4, num_vertices5);

  ASSERT_EQ(node1->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node2->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node3->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node4->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node5->getTransactionStatusCount(), 10005);

  std::cout << "All transactions received ..." << std::endl;

  std::cout << "Compute ordered dag blocks ..." << std::endl;
  auto block = node1->getConfig().chain.dag_genesis_block;
  std::vector<std::string> ghost;
  node1->getGhostPath(block.getHash().toString(), ghost);
  ASSERT_GT(ghost.size(), 1);
  uint64_t period = 0, cur_period;
  std::shared_ptr<vec_blk_t> order;
  auto anchor = blk_hash_t(ghost.back());
  std::tie(cur_period, order) = node1->getDagBlockOrder(anchor);
  ASSERT_TRUE(order);
  EXPECT_GT(order->size(), 5);
  std::cout << "Ordered dag block chain size: " << order->size() << std::endl;

  auto dag_size = node1->getNumVerticesInDag();

  // can have multiple dummy blocks
  auto vertices_diff = node1->getNumVerticesInDag().first - 1 - order->size();
  if (vertices_diff < 0 || vertices_diff >= 5) {
    node1->drawGraph("debug_dag");
    for (auto i(0); i < order->size(); ++i) {
      auto blk = (*order)[i];
      std::cout << i << " " << blk << " "
                << " trx: " << node1->getDagBlock(blk)->getTrxs().size()
                << std::endl;
    }
  }

  // diff should be larger than 0 but smaller than number of nodes
  // genesis block won't be executed
  EXPECT_LT(vertices_diff, 5)
      << " Number of vertices: " << node1->getNumVerticesInDag().first
      << " Number of ordered blks: " << order->size() << std::endl;
  EXPECT_GE(vertices_diff, 0)
      << " Number of vertices: " << node1->getNumVerticesInDag().first
      << " Number of ordered blks: " << order->size() << std::endl;

  auto overlap_table = node1->computeTransactionOverlapTable(order);
  // check transaction overlapping ...
  auto trx_table = node1->getUnsafeTransactionStatusTable();
  auto trx_table2 = trx_table;
  std::unordered_set<trx_hash_t> ordered_trxs;
  std::unordered_set<trx_hash_t> packed_trxs;
  uint all_trxs = 0;
  for (auto const &entry : *overlap_table) {
    auto const &blk = entry.first;
    auto const &overlap_vec = entry.second;
    auto dag_blk = node1->getDagBlock(blk);
    ASSERT_NE(dag_blk, nullptr);
    auto trxs = dag_blk->getTrxs();
    ASSERT_TRUE(trxs.size() == overlap_vec.size());
    all_trxs += overlap_vec.size();

    for (auto i = 0; i < trxs.size(); i++) {
      auto trx = trxs[i];
      if (!ordered_trxs.count(trx)) {
        EXPECT_TRUE(overlap_vec[i]);
        ordered_trxs.insert(trx);
        auto trx_status = trx_table[trx];
        EXPECT_EQ(trx_status, TransactionStatus::in_block)
            << "Error: " << trx << " status " << asInteger(trx_status)
            << std::endl;

      } else {
        EXPECT_FALSE(overlap_vec[i]);
      }
      auto it = trx_table2.find(trx);
      if (it != trx_table2.end()) {
        trx_table2.erase(it);
      }
      packed_trxs.insert(trx);
    }
  }
  for (auto const &t : trx_table2) {
    auto trx = node1->getTransaction(t.first);
    assert(trx);
    std::cout << "Unpacked trx: " << (*trx).first.getHash()
              << " sender: " << (*trx).first.getSender()
              << " nonce: " << (*trx).first.getNonce() << std::endl;
  }

  std::cout << "Total packed (overlapped) trxs: " << all_trxs << std::endl;
  EXPECT_GT(all_trxs, 10005);
  ASSERT_EQ(ordered_trxs.size(), 10005)
      << "Number of unpacked_trx " << trx_table2.size() << std::endl
      << "Total packed (non-overlapped) trxs " << packed_trxs.size()
      << std::endl;
}

TEST_F(FullNodeTest, transfer_to_self) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;
  auto node1 = top1.getNode();
  std::cout << "Send first trx ..." << std::endl;
  auto node_addr = node1->getAddress();
  auto initial_bal = node1->getBalance(node_addr);
  auto trx_count(100);
  EXPECT_TRUE(initial_bal.second);
  system(fmt(R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0",
  "method": "create_test_coin_transactions",
  "params": [{
    "secret":"3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    "delay": 5,
    "number": %s,
    "nonce": 0,
    "receiver": "%s"}]}' 0.0.0.0:7777)",
             trx_count, node_addr)
             .data());
  thisThreadSleepForSeconds(5);
  EXPECT_EQ(node1->getTransactionStatusCount(), trx_count);
  auto trx_executed1 = node1->getNumTransactionExecuted();
  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == trx_count) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, trx_count);
  auto const bal = node1->getBalance(node_addr);
  EXPECT_TRUE(bal.second);
  EXPECT_EQ(bal.first, initial_bal.first);
}

TEST_F(FullNodeTest,
       DISABLED_transaction_failure_does_not_cause_block_failure) {
  // TODO move to another file
  using namespace std;
  auto node = FullNode::make("./core_tests/conf/conf_taraxa1.json");
  node->setDebug(true);
  node->start(true);
  thisThreadSleepForMilliSeconds(500);
  vector<Transaction> transactions;
  transactions.emplace_back(0, 100, 0, samples::TEST_TX_GAS_LIMIT, addr(),
                            bytes(), node->getSecretKey());
  // This must cause out of balance error
  transactions.emplace_back(1, node->getMyBalance() + 100, 0,
                            samples::TEST_TX_GAS_LIMIT, addr(), bytes(),
                            node->getSecretKey());
  for (auto const &trx : transactions) {
    node->insertTransaction(trx);
  }
  auto trx_executed = node->getNumTransactionExecuted();
  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    if (trx_executed == transactions.size()) break;
    thisThreadSleepForMilliSeconds(100);
    trx_executed = node->getNumTransactionExecuted();
  }
  EXPECT_EQ(trx_executed, transactions.size())
      << "Trx executed: " << trx_executed
      << " ,Trx size: " << transactions.size();
}

TEST_F(FullNodeTest, DISABLED_mem_usage) {
  Top top1(6, input1);
  std::cout << "Top1 created ..." << std::endl;

  auto node1 = top1.getNode();
  // send 1000 trxs
  try {
    std::string sendtrx1 =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 200, 
                                      "number": 1000000, 
                                      "nonce": 0, 
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777)";

    std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
    t1.join();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  uint64_t last_num_block_proposed = 0;
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    auto res = getMemUsage("full_node_test");
    std::cout << "Mem usage (" << i * 5 << ") in sec = " << res << " M"
              << std::endl;
    taraxa::thisThreadSleepForMilliSeconds(5000);
    auto cur_num_block_proposed = node1->getNumProposedBlocks();
    if (cur_num_block_proposed == last_num_block_proposed) break;
    last_num_block_proposed = cur_num_block_proposed;
  }
}

}  // namespace taraxa

int main(int argc, char **argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  // logOptions.includeChannels.push_back("FULLND");
  // logOptions.includeChannels.push_back("BLKQU");
  // logOptions.includeChannels.push_back("TARCAP");
  // logOptions.includeChannels.push_back("DAGSYNC");
  // logOptions.includeChannels.push_back("DAGPRP");
  // logOptions.includeChannels.push_back("TRXPRP");
  // logOptions.includeChannels.push_back("DAGMGR");
  // logOptions.includeChannels.push_back("TRXMGR");

  // logOptions.includeChannels.push_back("EXETOR");
  // logOptions.includeChannels.push_back("BLK_PP");
  // logOptions.includeChannels.push_back("PR_MDL");
  // logOptions.includeChannels.push_back("PBFT_MGR");
  // logOptions.includeChannels.push_back("PBFT_CHAIN");

  dev::setupLogging(logOptions);
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}