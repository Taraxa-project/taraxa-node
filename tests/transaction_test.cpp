#include "transaction/transaction.hpp"

#include <gtest/gtest.h>
#include <libdevcore/CommonJS.h>

#include <thread>
#include <vector>

#include "common/static_init.hpp"
#include "config/genesis.hpp"
#include "final_chain/trie_common.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_manager.hpp"
#include "test_util/samples.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/transaction_queue.hpp"

namespace taraxa::core_tests {

const unsigned NUM_TRX = 40;
const unsigned NUM_BLK = 4;
const unsigned BLK_TRX_LEN = 4;
const unsigned BLK_TRX_OVERLAP = 1;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(1, NUM_TRX, g_secret); });
auto g_blk_samples = Lazy([] { return samples::createMockDagBlkSamples(0, NUM_BLK, 0, BLK_TRX_LEN, BLK_TRX_OVERLAP); });

struct TransactionTest : NodesTest {};

TEST_F(TransactionTest, status_table_lru) {
  using TestStatus = StatusTable<int, int>;
  TestStatus status_table(100);
  for (int i = 0; i < 100; i++) {
    status_table.insert(i, i * 10);
  }
  for (int i = 0; i < 50; ++i) {
    status_table.get(i);
  }
  for (int i = 101; i < 150; i++) {
    status_table.insert(i, i * 10);
  }
  for (int i = 50; i < 100; ++i) {
    auto [exist, val] = status_table.get(51);
    EXPECT_FALSE(exist);
  }
  EXPECT_LE(status_table.size(), 100);
}

TEST_F(TransactionTest, sig) {
  /* The test data was generated via web3.py v5.12.0 in the following way
   * in the order of appearance:
  import web3
  acc = web3.Account.from_key(
    "0xb6a431fe9f5a79da0c53702ca5dd619846e21ce5c3b7b77148aa8478240087cd")
  print(acc.privateKey.hex())
  print(acc.address)
  print(acc.sign_transaction(dict(chainId=0, nonce=0, gas=0,
    gasPrice=0)).rawTransaction.hex())
  for chain_id in [None, 1, 1 << 32]:
      print(acc.sign_transaction(dict(
          nonce=1,
          value=2,
          gasPrice=3,
          gas=4,
          data='0xabcd',
          to='0xd3CdA913deB6f67967B99D67aCDFa1712C293601',
          chainId=chain_id
      )).hash.hex())
   */
  secret_t sk("0xb6a431fe9f5a79da0c53702ca5dd619846e21ce5c3b7b77148aa8478240087cd");
  addr_t sender("0x9DeAC03F89e7819a241Ee3856715A1Ab76248023");
  ASSERT_THROW(Transaction(dev::jsToBytes("0xf84980808080808024a01404adc97c8b58fef303b2862d0e72378"
                                          "4fb635e7237e0e8d3ea33bbea19c36ca0229e80d57ba91a0f347686"
                                          "30fd21ad86e4c403b307de9ac4550d0ccc81c90fe3")),
               Transaction::InvalidFormat);
  std::vector<std::pair<uint64_t, string>> valid_cases{
      {0, "0xf647d1d47ce927ce2fb9f57e4e2a3c32b037c5e544b44611077f5cc6980b0bc2"},
      {1, "0x49c1cb845df5d3ed238ca37ad25ca96f417e4f22d7911224cf3c2a725985e7ff"},
      {uint64_t(1) << uint(32), "0xc1651c53d21ad6ddaac0af7ad93947074ef9f3b03479a36b29fa577b9faba8a9"},
  };
  for (auto& [chain_id, hash_str] : valid_cases) {
    auto t = std::make_shared<Transaction>(1, 2, 3, 4, dev::jsToBytes("0xabcd"), sk,
                                           addr_t("0xd3CdA913deB6f67967B99D67aCDFa1712C293601"), chain_id);
    for (auto i : {1, 0}) {
      ASSERT_EQ(t->getSender(), sender);
      ASSERT_EQ(t->getChainID(), chain_id);
      ASSERT_EQ(t->getHash(), h256(hash_str));
      if (i) {
        t = std::make_shared<Transaction>(t->rlp());
        continue;
      }
      dev::RLPStream with_modified_payload(9);
      dev::RLPStream with_invalid_signature(9);
      uint fields_processed = 0;
      for (auto const el : dev::RLP(t->rlp())) {
        if (auto el_modified = el.toBytes(); ++fields_processed <= 6) {
          for (auto& b : el_modified) {
            b = ~b;
          }
          with_modified_payload << el_modified;
          with_invalid_signature << el.toBytes();
        } else {
          for (auto& b : el_modified) {
            b = 0;
          }
          with_modified_payload << el.toBytes();
          with_invalid_signature << el_modified;
        }
      }
      ASSERT_NE(Transaction(with_modified_payload.out()).getSender(), sender);
      ASSERT_THROW(Transaction(with_invalid_signature.out()).getSender(), Transaction::InvalidTransaction);
    }
  }
}

TEST_F(TransactionTest, verifiers) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  auto final_chain = NewFinalChain(db, cfg);
  TransactionManager trx_mgr(cfg, db, final_chain, addr_t());
  // insert trx
  std::thread t([&trx_mgr]() {
    for (auto const& t : *g_signed_trx_samples) {
      trx_mgr.insertTransaction(t);
    }
  });

  // insert trx again, should not duplicated
  for (auto const& t : *g_signed_trx_samples) {
    trx_mgr.insertTransaction(t);
  }
  t.join();
  thisThreadSleepForMilliSeconds(100);
  EXPECT_EQ(trx_mgr.getTransactionPoolSize(), g_signed_trx_samples->size());
}

TEST_F(TransactionTest, transaction_limit) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  TransactionManager trx_mgr(cfg, db, NewFinalChain(db, cfg), addr_t());
  // insert trx
  std::thread t([&trx_mgr]() {
    for (auto const& t : *g_signed_trx_samples) {
      trx_mgr.insertTransaction(t);
    }
  });

  // insert trx again, should not duplicated
  for (auto const& t : *g_signed_trx_samples) {
    trx_mgr.insertTransaction(t);
  }
  t.join();
  thisThreadSleepForMilliSeconds(100);
  auto verified_trxs = trx_mgr.getAllPoolTrxs();
  EXPECT_EQ(verified_trxs[0].size(), g_signed_trx_samples->size());
}

TEST_F(TransactionTest, prepare_signed_trx_for_propose) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  TransactionManager trx_mgr(cfg, db, NewFinalChain(db, cfg), addr_t());
  std::thread insertTrx([&trx_mgr]() {
    for (auto const& t : *g_signed_trx_samples) {
      trx_mgr.insertTransaction(t);
    }
  });

  thisThreadSleepForMilliSeconds(500);

  insertTrx.join();
  SharedTransactions total_packed_trxs;
  std::vector<SharedTransactions> packed_trxs;
  std::cout << "Start block proposing ..." << std::endl;
  std::thread wakeup([]() { thisThreadSleepForSeconds(2); });
  auto batch = db->createWriteBatch();

  do {
    packed_trxs = trx_mgr.getAllPoolTrxs();
    if (packed_trxs.empty()) break;
    total_packed_trxs.insert(total_packed_trxs.end(), packed_trxs[0].begin(), packed_trxs[0].end());
    trx_mgr.saveTransactionsFromDagBlock(packed_trxs[0]);
    thisThreadSleepForMicroSeconds(100);
  } while (!packed_trxs.empty());
  wakeup.join();
  EXPECT_EQ(total_packed_trxs.size(), NUM_TRX) << " Packed Trx: " << ::testing::PrintToString(total_packed_trxs);
}

TEST_F(TransactionTest, transaction_low_nonce) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  auto final_chain = NewFinalChain(db, cfg);
  TransactionManager trx_mgr(cfg, db, final_chain, addr_t());
  const auto& trx_2 = g_signed_trx_samples[1];
  auto& trx_1 = g_signed_trx_samples[0];

  // Insert and execute transaction with nonce 1, 2
  EXPECT_TRUE(trx_mgr.insertTransaction(trx_1).first);
  EXPECT_TRUE(trx_mgr.insertTransaction(trx_2).first);
  std::vector<trx_hash_t> trx_hashes{trx_1->getHash(), trx_2->getHash()};
  DagBlock dag_blk({}, {}, {}, trx_hashes, secret_t::random());
  db->saveDagBlock(dag_blk);
  std::vector<vote_hash_t> reward_votes_hashes;
  auto pbft_block =
      std::make_shared<PbftBlock>(kNullBlockHash, kNullBlockHash, kNullBlockHash, kNullBlockHash, 1, addr_t::random(),
                                  dev::KeyPair::create().secret(), std::move(reward_votes_hashes));
  PeriodData period_data(pbft_block, {});
  period_data.dag_blocks.push_back(dag_blk);
  SharedTransactions trxs{trx_1, trx_2};
  period_data.transactions = trxs;
  auto batch = db->createWriteBatch();
  db->savePeriodData(period_data, batch);
  db->commitWriteBatch(batch);
  final_chain->finalize(std::move(period_data), {dag_blk.getHash()}).get();

  // Verify low nonce transaction is detected in verification
  auto low_nonce_trx = std::make_shared<Transaction>(1, 101, 0, 100000, dev::bytes(), g_secret, addr_t::random());
  auto result = trx_mgr.verifyTransaction(low_nonce_trx);
  EXPECT_EQ(result.first, TransactionStatus::LowNonce);
  EXPECT_FALSE(trx_mgr.insertTransaction(low_nonce_trx).first);

  // Verify dag blocks will pass verification if contain low nonce transactions
  DagBlock dag_blk_with_low_nonce_transaction({}, {}, {}, {low_nonce_trx->getHash()}, secret_t::random());
  EXPECT_FALSE(trx_mgr.getBlockTransactions(dag_blk_with_low_nonce_transaction).has_value());
  trx_mgr.insertValidatedTransaction(std::move(low_nonce_trx), TransactionStatus::LowNonce);
  EXPECT_TRUE(trx_mgr.getBlockTransactions(dag_blk_with_low_nonce_transaction).has_value());

  // Verify insufficient balance transaction is detected in verification
  auto trx_insufficient_balance =
      std::make_shared<Transaction>(3, final_chain->get_account(dev::toAddress(g_secret))->balance + 1, 0, 100000,
                                    dev::bytes(), g_secret, addr_t::random());
  result = trx_mgr.verifyTransaction(trx_insufficient_balance);
  EXPECT_EQ(result.first, TransactionStatus::InsufficentBalance);
  EXPECT_FALSE(trx_mgr.insertTransaction(trx_insufficient_balance).first);

  // Verify dag blocks will pass verification if contain insufficient balance transactions
  DagBlock dag_blk_with_insufficient_balance_transaction({}, {}, {}, {trx_insufficient_balance->getHash()},
                                                         secret_t::random());
  EXPECT_FALSE(trx_mgr.getBlockTransactions(dag_blk_with_insufficient_balance_transaction).has_value());
  trx_mgr.insertValidatedTransaction(std::move(trx_insufficient_balance), TransactionStatus::InsufficentBalance);
  EXPECT_TRUE(trx_mgr.getBlockTransactions(dag_blk_with_insufficient_balance_transaction).has_value());

  trx_mgr.blockFinalized(11);
  EXPECT_TRUE(trx_mgr.getBlockTransactions(dag_blk_with_low_nonce_transaction).has_value());
  EXPECT_TRUE(trx_mgr.getBlockTransactions(dag_blk_with_insufficient_balance_transaction).has_value());

  // Verify that after 10 executed blocks transactions expire
  trx_mgr.blockFinalized(12);
  EXPECT_FALSE(trx_mgr.getBlockTransactions(dag_blk_with_low_nonce_transaction).has_value());
  EXPECT_FALSE(trx_mgr.getBlockTransactions(dag_blk_with_insufficient_balance_transaction).has_value());
}

TEST_F(TransactionTest, transaction_concurrency) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  TransactionManager trx_mgr(cfg, db, NewFinalChain(db, cfg), addr_t());
  bool stopped = false;
  // Insert transactions to memory pool and keep trying to insert them again on separate thread, it should always fail
  std::thread insertTrx([&trx_mgr, &stopped]() {
    for (auto const& t : *g_signed_trx_samples) {
      trx_mgr.insertTransaction(t);
    }
    while (!stopped) {
      for (auto const& t : *g_signed_trx_samples) {
        EXPECT_FALSE(trx_mgr.insertTransaction(t).first);
      }
    }
  });

  // 2/3 of transactions removed from pool and saved to db
  for (size_t i = 0; i < g_signed_trx_samples->size() * 2 / 3; i++) {
    trx_mgr.saveTransactionsFromDagBlock({g_signed_trx_samples[i]});
  }

  // 1/3 transactions finalized
  for (size_t i = 0; i < g_signed_trx_samples->size() / 3; i++) {
    db->saveTransactionPeriod(g_signed_trx_samples[i]->getHash(), 1, i);
    PeriodData period_data;
    period_data.transactions = {g_signed_trx_samples[i]};
    trx_mgr.updateFinalizedTransactionsStatus(period_data);
  }

  // Stop the thread which is trying to indert transactions to pool
  stopped = true;
  insertTrx.join();

  // Verify all transactions are in correct state in pool, db and finalized
  std::set<trx_hash_t> pool_trx_hashes;
  for (auto const& account_t : trx_mgr.getAllPoolTrxs()) {
    for (auto const& t : account_t) {
      pool_trx_hashes.insert(t->getHash());
    }
  }

  EXPECT_EQ(pool_trx_hashes.size(), g_signed_trx_samples->size() - g_signed_trx_samples->size() * 2 / 3);

  for (size_t i = 0; i < g_signed_trx_samples->size() / 3; i++) {
    EXPECT_FALSE(pool_trx_hashes.count(g_signed_trx_samples[i]->getHash()) > 0);
    EXPECT_TRUE(db->transactionInDb(g_signed_trx_samples[i]->getHash()));
    EXPECT_TRUE(db->transactionFinalized(g_signed_trx_samples[i]->getHash()));
  }

  for (size_t i = g_signed_trx_samples->size() / 3; i < g_signed_trx_samples->size() * 2 / 3; i++) {
    EXPECT_FALSE(pool_trx_hashes.count(g_signed_trx_samples[i]->getHash()) > 0);
    EXPECT_TRUE(db->transactionInDb(g_signed_trx_samples[i]->getHash()));
    EXPECT_FALSE(db->transactionFinalized(g_signed_trx_samples[i]->getHash()));
  }

  for (size_t i = g_signed_trx_samples->size() * 2 / 3; i < g_signed_trx_samples->size(); i++) {
    EXPECT_TRUE(pool_trx_hashes.count(g_signed_trx_samples[i]->getHash()) > 0);
    EXPECT_FALSE(db->transactionInDb(g_signed_trx_samples[i]->getHash()));
  }
}

TEST_F(TransactionTest, priority_queue) {
  // Check ordering by same sender and different nonce
  {
    TransactionQueue priority_queue;
    uint32_t nonce = 0;
    auto trx = std::make_shared<Transaction>(nonce++, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                             addr_t::random());
    auto trx2 = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                              addr_t::random());
    const auto trx_hash = trx->getHash();
    priority_queue.insert(std::move(trx2), TransactionStatus::Verified, 1);
    priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1);
    EXPECT_EQ(priority_queue.getOrderedTransactions(1)[0]->getHash(), trx_hash);
    EXPECT_EQ(priority_queue.size(), 2);
  }

  // Check double insertion
  {
    TransactionQueue priority_queue;
    uint32_t nonce = 0;
    auto trx = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                             addr_t::random());
    auto trx2 = trx;
    EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
    EXPECT_FALSE(priority_queue.insert(std::move(trx2), TransactionStatus::Verified, 1));
    EXPECT_EQ(priority_queue.size(), 1);
  }

  // Check ordering by same sender, same nonce but different gas_price
  {
    TransactionQueue priority_queue;
    uint32_t nonce = 0;
    auto trx = std::make_shared<Transaction>(nonce, 1, 10, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                             addr_t::random());
    auto trx2 = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                              addr_t::random());
    auto trx_hash = trx->getHash();
    priority_queue.insert(std::move(trx2), TransactionStatus::Verified, 1);
    priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1);
    EXPECT_EQ(priority_queue.getOrderedTransactions(1)[0]->getHash(), trx_hash);
    EXPECT_EQ(priority_queue.size(), 1);
  }

  /*
  sender A:
  - TXA1, nonce 1, fee 5
  - TXA2, nonce 2, fee 6
  sender B
  - TXB1, nonce 1, fee 4
  Should be TXA1, TXB1, TXA2
  */
  {
    TransactionQueue priority_queue;
    auto trxa1 = std::make_shared<Transaction>(1 /*nonce*/, 1, 5 /*fee*/, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                               g_secret, addr_t::random());
    auto trxa2 = std::make_shared<Transaction>(2 /*nonce*/, 1, 6 /*fee*/, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                               g_secret, addr_t::random());
    auto trxb1 = std::make_shared<Transaction>(1 /*nonce*/, 1, 4 /*fee*/, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                               secret_t::random(), addr_t::random());
    auto trxa1_hash = trxa1->getHash();
    auto trxa2_hash = trxa2->getHash();
    auto trxb1_hash = trxb1->getHash();

    priority_queue.insert(std::move(trxb1), TransactionStatus::Verified, 1);
    priority_queue.insert(std::move(trxa2), TransactionStatus::Verified, 1);
    priority_queue.insert(std::move(trxa1), TransactionStatus::Verified, 1);
    EXPECT_EQ(priority_queue.size(), 3);
    EXPECT_EQ(priority_queue.getOrderedTransactions(3)[0]->getHash(), trxa1_hash);
    EXPECT_EQ(priority_queue.getOrderedTransactions(3)[1]->getHash(), trxa2_hash);
    EXPECT_EQ(priority_queue.getOrderedTransactions(3)[2]->getHash(), trxb1_hash);
  }
}

TEST_F(TransactionTest, priority_queue_max_size) {
  // Check if insertion working as expected
  {
    const uint32_t max_queue_size = 100;
    TransactionQueue priority_queue(max_queue_size);
    uint32_t nonce = 0;
    for (uint32_t i = 0; i < max_queue_size + 1; i++) {
      auto trx = std::make_shared<Transaction>(nonce++, 1, 2, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                               dev::KeyPair::create().secret(), addr_t::random());
      if (i > 96) {
        trx = std::make_shared<Transaction>(nonce++, 1, 1, 99, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                            addr_t::random());
      }
      if (i < max_queue_size) {
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      } else {
        auto trx_hash = trx->getHash();
        EXPECT_FALSE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
        EXPECT_EQ(priority_queue.get(trx_hash), nullptr);
      }
    }
    EXPECT_EQ(priority_queue.size(), max_queue_size);
  }
  // Check if insertion working as expected when trx should be replaced
  {
    const uint32_t max_queue_size = 100;
    TransactionQueue priority_queue(max_queue_size);
    uint32_t nonce = 0;
    trx_hash_t hash_to_remove;
    for (uint32_t i = 0; i < max_queue_size + 1; i++) {
      if (i == 3) {
        auto trx = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                                 addr_t::random());
        hash_to_remove = trx->getHash();
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      } else if (i == 4) {
        auto trx = std::make_shared<Transaction>(nonce++, 1, 2, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                                 addr_t::random());
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      } else {
        auto trx = std::make_shared<Transaction>(nonce++, 1, 2, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                                 dev::KeyPair::create().secret(), addr_t::random());
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      }
    }

    // Transaction is still part of non proposable transactions
    EXPECT_EQ(priority_queue.get(hash_to_remove)->getHash(), hash_to_remove);
    EXPECT_TRUE(priority_queue.isTransactionKnown(hash_to_remove));
    EXPECT_EQ(priority_queue.size(), max_queue_size);

    // Confirm that transaction is not proposable
    auto trxs = priority_queue.getOrderedTransactions(max_queue_size * 2);
    EXPECT_EQ(trxs.size(), max_queue_size);
    for (const auto& t : trxs) {
      EXPECT_TRUE(t->getHash() != hash_to_remove);
    }
  }
  // Check if Forced insertion working as expected
  {
    const uint32_t max_queue_size = 100;
    TransactionQueue priority_queue(max_queue_size);
    uint32_t nonce = 0;
    trx_hash_t hash_to_remove;
    for (uint32_t i = 0; i < max_queue_size + 1; i++) {
      if (i == 3) {
        auto trx = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                                 addr_t::random());
        hash_to_remove = trx->getHash();
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Forced, 1));
      } else if (i == 4) {
        auto trx = std::make_shared<Transaction>(nonce++, 1, 2, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                                 addr_t::random());
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      } else {
        auto trx = std::make_shared<Transaction>(nonce++, 1, 2, 100, dev::fromHex("00FEDCBA9876543210000000"),
                                                 dev::KeyPair::create().secret(), addr_t::random());
        EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Verified, 1));
      }
      EXPECT_FALSE(priority_queue.nonProposableTransactionsOverTheLimit());
    }

    EXPECT_TRUE(priority_queue.get(hash_to_remove) != nullptr);
    EXPECT_TRUE(priority_queue.isTransactionKnown(hash_to_remove));
    EXPECT_EQ(priority_queue.size(), max_queue_size);

    for (uint32_t i = 2; i < max_queue_size + 1; i++) {
      auto trx = std::make_shared<Transaction>(nonce, 1, 1, 100, dev::fromHex("00FEDCBA9876543210000000"), g_secret,
                                               addr_t::random());
      EXPECT_TRUE(priority_queue.insert(std::move(trx), TransactionStatus::Forced, 1));
      if (i < max_queue_size * 20 / 100) {
        EXPECT_FALSE(priority_queue.nonProposableTransactionsOverTheLimit());
      } else {
        EXPECT_TRUE(priority_queue.nonProposableTransactionsOverTheLimit());
      }
    }
  }
}

SharedTransactions generateRandomOrderTransactions(uint32_t size) {
  SharedTransactions trxs;
  std::vector<dev::KeyPair> kpv;
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist10(0, 9);
  for (auto i = 0; i <= 10; ++i) {
    kpv.push_back(dev::KeyPair::create());
  }
  for (uint32_t i = 0; i < size; ++i) {
    trxs.emplace_back(std::make_shared<Transaction>(dist10(rng), 100, 21000 + dist10(rng), 100000, dev::bytes(),
                                                    kpv[dist10(rng)].secret(), addr_t::random()));
  }
  return trxs;
}

TEST_F(TransactionTest, priority_queue_ordering) {
  // Test generates 1000 transactions from 10 random accounts with random nonces between 1 and 10 and random gas proces
  // and verified that transactions are properly sorted in transaction queue and that all the duplicate transactions
  // from same account and same nonce are removed with always keeping the transaction with highest gas price
  const uint32_t number_of_runs = 30;
  for (uint32_t i = 0; i < number_of_runs; i++) {
    const uint32_t max_queue_size = 1000;
    TransactionQueue priority_queue(max_queue_size);
    auto trxs = generateRandomOrderTransactions(max_queue_size);
    // Calculate number of unique account nonce transactions
    std::unordered_map<addr_t, std::map<val_t, val_t>> account_nonces_max_gas_price;
    for (auto& t : trxs) {
      auto it = account_nonces_max_gas_price.find(t->getSender());
      if (it != account_nonces_max_gas_price.end()) {
        auto it2 = it->second.find(t->getNonce());
        if (it2 != it->second.end()) {
          if (it2->second < t->getGasPrice()) {
            it2->second = t->getGasPrice();
          }
        } else {
          it->second[t->getNonce()] = t->getGasPrice();
        }
      } else {
        account_nonces_max_gas_price[t->getSender()][t->getNonce()] = t->getGasPrice();
      }
      EXPECT_TRUE(priority_queue.insert(std::move(t), TransactionStatus::Verified, 1));
    }
    uint32_t unique_count = 0;
    for (auto it : account_nonces_max_gas_price) {
      for (auto it2 : it.second) {
        unique_count++;
      }
    }

    // Verify that only unique transactions are actually ordered in queue
    EXPECT_EQ(priority_queue.size(), unique_count);
    auto ordered_trxs = priority_queue.getOrderedTransactions(max_queue_size);
    // Verify only unique nonce transactions are ordered
    EXPECT_EQ(ordered_trxs.size(), unique_count);

    // Verify nonce order
    std::unordered_map<addr_t, val_t> account_nonces;
    for (auto t : ordered_trxs) {
      if (account_nonces.contains(t->getSender())) {
        EXPECT_GT(t->getNonce(), account_nonces[t->getSender()]);
      }
      account_nonces[t->getSender()] = t->getNonce();
    }
    // Verify gasprice order up to the point where account appears for the second time
    std::unordered_set<addr_t> accounts;
    for (uint32_t i = 1; i < ordered_trxs.size(); i++) {
      accounts.insert(ordered_trxs[i - 1]->getSender());
      if (!accounts.contains(ordered_trxs[i]->getSender())) {
        EXPECT_LE(ordered_trxs[i]->getGasPrice(), ordered_trxs[i - 1]->getGasPrice());
      }
    }
  }
}

TEST_F(TransactionTest, finalization_ordering) {
  // Test generates 1000 transactions from 10 random accounts with random nonces between 1 and 10 and random gas proces
  // and verified that transactions are properly sorted in transaction queue and that all the duplicate transactions
  // from same account and same nonce are removed with always keeping the transaction with highest gas price
  const uint32_t number_of_runs = 30;
  for (uint32_t i = 0; i < number_of_runs; i++) {
    const uint32_t max_queue_size = 1000;
    TransactionQueue priority_queue(max_queue_size);
    auto trxs = generateRandomOrderTransactions(max_queue_size);

    EXPECT_EQ(trxs.size(), max_queue_size);
    PbftManager::reorderTransactions(trxs);
    EXPECT_EQ(trxs.size(), max_queue_size);

    std::unordered_map<addr_t, val_t> account_nonces;
    for (auto& t : trxs) {
      auto it = account_nonces.find(t->getSender());
      if (it != account_nonces.end()) {
        EXPECT_GE(t->getNonce(), it->second);
        it->second = t->getNonce();
      } else {
        account_nonces[t->getSender()] = t->getNonce();
      }
    }
  }
}

TEST_F(TransactionTest, priority_queue_ordering_eth_test) {
  SharedTransactions trxs;
  std::vector<dev::KeyPair> kpv;
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist10(0, 9);
  for (auto i = 0; i <= 25; ++i) {
    kpv.push_back(dev::KeyPair::create());
  }

  for (auto start = 0; start < 25; start++) {
    for (auto i = 0; i <= 25; ++i) {
      trxs.emplace_back(std::make_shared<Transaction>((val_t)(start + i), 100, 21000 + start + i, 100000, dev::bytes(),
                                                      kpv[start].secret(), addr_t::random()));
    }
  }

  TransactionQueue priority_queue(1000);
  for (auto& t : trxs) {
    EXPECT_TRUE(priority_queue.insert(std::move(t), TransactionStatus::Verified, 1));
  }

  trxs = priority_queue.getOrderedTransactions(1000);
  for (auto i = 0; i < (int32_t)trxs.size(); ++i) {
    // Make sure the nonce order is valid
    for (auto j = i + 1; j < (int32_t)trxs.size(); ++j) {
      if (trxs[i]->getSender() == trxs[j]->getSender()) {
        EXPECT_LT(trxs[i]->getNonce(), trxs[j]->getNonce());
      }
    }

    // Find the previous and next nonce of this account
    auto prev = i - 1;
    auto next = i + 1;

    for (auto j = i - 1; j >= 0; j--) {
      if (trxs[i]->getSender() == trxs[j]->getSender()) {
        prev = j;
        break;
      }
    }

    for (auto j = i + 1; j < (int32_t)trxs.size(); j++) {
      if (trxs[i]->getSender() == trxs[j]->getSender()) {
        next = j;
        break;
      }
    }

    // Make sure that in between the neighbor nonces, the transaction is correctly positioned price wise
    for (auto j = prev + 1; j < next; j++) {
      if (j < i) {
        EXPECT_GE(trxs[j]->getGasPrice(), trxs[i]->getGasPrice());
      }
      if (j > i) {
        EXPECT_GE(trxs[i]->getGasPrice(), trxs[j]->getGasPrice());
      }
    }
  }
}

TEST_F(TransactionTest, typed_deserialization) {
  auto trx_rlp =
      "0x01f88380018203339407a565b7ed7d7a678680a4c162885bedbb695fe080a44401a6e40000000000000000000000000000000000000000"
      "00000000000000000000001226a0223a7c9bcf5531c99be5ea7082183816eb20cfe0bbc322e97cc5c7f71ab8b20ea02aadee6b34b45bb15b"
      "c42d9c09de4a6754e7000908da72d48cc7704971491663";
  try {
    Transaction(jsToBytes(trx_rlp, dev::OnFailed::Throw), true);
  } catch (const std::exception& e) {
    const std::string exception_str = e.what();
    EXPECT_TRUE(exception_str.find("Can't parse transaction from RLP. Use legacy transactions because typed "
                                   "transactions aren't supported yet.") != string::npos);
    return;
  }
  // shouldn't reach this code
  GTEST_FAIL();
}
TEST_F(TransactionTest, zero_gas_price_limit) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  auto final_chain = NewFinalChain(db, cfg);
  TransactionManager trx_mgr(cfg, db, final_chain, addr_t());
  auto make_trx_with_price = [](uint64_t price) {
    return std::make_shared<Transaction>(1, 100, price, 100000, dev::bytes(), g_secret, addr_t::random());
  };

  // OK: gas_price == minimum_price
  {
    auto res = trx_mgr.insertTransaction(make_trx_with_price(0));
    ASSERT_TRUE(res.first);
  }

  // OK: gas_price > minimum_price
  {
    auto res = trx_mgr.insertTransaction(make_trx_with_price(1));
    ASSERT_TRUE(res.first);
  }
}

TEST_F(TransactionTest, gas_price_limiting) {
  auto db = std::make_shared<DbStorage>(data_dir);
  auto cfg = node_cfgs.front();
  auto minimum_price = cfg.genesis.gas_price.minimum_price = 10;
  auto final_chain = NewFinalChain(db, cfg);
  TransactionManager trx_mgr(cfg, db, final_chain, addr_t());
  auto make_trx_with_price = [](uint64_t price) {
    return std::make_shared<Transaction>(1, 100, price, 100000, dev::bytes(), g_secret, addr_t::random());
  };

  // FAIL: gas_price < minimum_price
  {
    auto res = trx_mgr.insertTransaction(make_trx_with_price(minimum_price - 1));
    ASSERT_FALSE(res.first);
    ASSERT_EQ(res.second, "gas_price too low");
  }

  // OK: gas_price == minimum_price
  {
    auto res = trx_mgr.insertTransaction(make_trx_with_price(minimum_price));
    ASSERT_TRUE(res.first);
  }

  // OK: gas_price > minimum_price
  {
    auto res = trx_mgr.insertTransaction(make_trx_with_price(minimum_price + 1));
    ASSERT_TRUE(res.first);
  }
}

TEST_F(TransactionTest, signature_performance) {
  auto now = std::chrono::steady_clock::now();
  auto trxs = samples::createSignedTrxSamples(1, 1000, g_secret);
  std::cout << "Time to create 1000 transactions: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count()
            << "ms" << std::endl;
  SharedTransactions trxs_from_rlp;
  for (auto t : trxs) {
    trxs_from_rlp.push_back(std::make_shared<Transaction>(t->rlp()));
  }
  now = std::chrono::steady_clock::now();
  for (auto& t : trxs_from_rlp) {
    t->getSender();
  }
  std::cout << "Time to verify 1000 transactions signature: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count()
            << "ms" << std::endl;
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
