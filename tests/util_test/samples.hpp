#pragma once

#include <fstream>
#include <map>
#include <string>

#include "common/lazy.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"
#include "util.hpp"

namespace taraxa::core_tests::samples {

class TxGenerator {
 public:
  // this function guarantees uniqueness for generated values
  // scoped to the class instance
  auto getRandomUniqueSenderSecret() const {
    std::unique_lock l(mutex_);
    auto secret = secret_t::random();
    while (!used_secrets_.insert(secret.makeInsecure()).second) {
      secret = secret_t::random();
    }
    return secret;
  }

  auto getWithRandomUniqueSender(val_t const &value = 0, addr_t const &to = addr_t::random(),
                                 bytes const &data = str2bytes("00FEDCBA9876543210000000")) const {
    return Transaction(0, value, 0, TEST_TX_GAS_LIMIT, data, getRandomUniqueSenderSecret(), to);
  }
  auto getSerialTrxWithSameSender(uint trx_num, uint64_t const &start_nonce, val_t const &value,
                                  addr_t const &receiver = addr_t::random()) const {
    std::vector<Transaction> trxs;
    for (auto i = start_nonce; i < start_nonce + trx_num; ++i) {
      trxs.emplace_back(Transaction(i, value, 0, TEST_TX_GAS_LIMIT, str2bytes("00FEDCBA9876543210000000"),
                                    getRandomUniqueSenderSecret(), receiver));
    }
    return trxs;
  }

 private:
  mutable std::mutex mutex_;
  mutable std::unordered_set<dev::FixedHash<secret_t::size>> used_secrets_;
};

inline auto const TX_GEN = Lazy([] { return TxGenerator(); });

inline bool sendTrx(uint64_t count, unsigned port) {
  auto pattern = R"(
      curl --silent -m 10 --output /dev/null -d \
      '{
        "jsonrpc": "2.0",
        "method": "send_coin_transaction",
        "id": "0",
        "params": [
          {
            "nonce": 0,
            "value": 0,
            "gas": "%s",
            "gas_price": "%s",
            "receiver": "%s",
            "secret": "%s"
          }
        ]
      }' 0.0.0.0:%s
    )";
  for (uint64_t i = 0; i < count; ++i) {
    auto retcode = system(fmt(pattern, val_t(TEST_TX_GAS_LIMIT), val_t(0), addr_t::random(),
                              samples::TX_GEN->getRandomUniqueSenderSecret().makeInsecure(), port)
                              .c_str());
    if (retcode != 0) {
      return false;
    }
  }
  return true;
}

struct TestAccount {
  TestAccount() = default;
  TestAccount(int id, std::string const &sk, std::string const &pk, std::string const &addr)
      : id(id), sk(sk), pk(pk), addr(addr) {}
  friend std::ostream;
  int id;
  std::string sk;
  std::string pk;
  std::string addr;
};

inline std::ostream &operator<<(std::ostream &strm, TestAccount const &acc) {
  strm << "sk: " << acc.sk << "\npk: " << acc.pk << "\naddr: " << acc.addr << std::endl;
  return strm;
}

inline std::map<int, TestAccount> createTestAccountTable(std::string const &filename) {
  std::map<int, TestAccount> acc_table;
  std::ifstream file;
  file.open(filename);
  if (file.fail()) {
    std::cerr << "Error! Cannot open " << filename << std::endl;
    return acc_table;
  }

  std::string id, sk, pk, addr;

  while (file >> id >> sk >> pk >> addr) {
    if (id.empty()) break;
    if (sk.empty()) break;
    if (pk.empty()) break;
    if (addr.empty()) break;

    int idx = std::stoi(id);
    acc_table[idx] = TestAccount(idx, sk, pk, addr);
  }
  return acc_table;
}

inline std::vector<Transaction> createMockTrxSamples(unsigned start, unsigned num) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  std::vector<Transaction> trxs;
  for (auto i = start; i < num; ++i) {
    Transaction trx(i,                                      // nonce
                    3,                                      // value
                    4,                                      // gas_price
                    5,                                      // gas
                    str2bytes("00FEDCBA9876543210000000"),  // data
                    secret_t::random(),                     // secret
                    addr_t(i * 1000)                        // receiver
    );
    trxs.emplace_back(trx);
  }
  return trxs;
}

inline std::vector<std::shared_ptr<Transaction>> createSignedTrxSamples(
    unsigned start, unsigned num, secret_t const &sk, bytes data = str2bytes("00FEDCBA9876543210000000")) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  std::vector<std::shared_ptr<Transaction>> trxs;
  for (auto i = start; i < num; ++i) {
    blk_hash_t hash(i);
    trxs.emplace_back(std::make_shared<Transaction>(i, i * 100, 0, 1000000, data, sk, addr_t((i + 1) * 100)));
  }
  return trxs;
}

inline std::vector<DagBlock> createMockDagBlkSamples(unsigned pivot_start, unsigned blk_num, unsigned trx_start,
                                                     unsigned trx_len, unsigned trx_overlap) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<DagBlock> blks;
  unsigned trx = trx_start;
  for (auto i = pivot_start; i < blk_num; ++i) {
    blk_hash_t pivot(i);
    blk_hash_t hash(i + 1);
    vec_trx_t trxs;
    for (unsigned i = 0; i < trx_len; ++i, trx++) {
      trxs.emplace_back(trx_hash_t(trx));
    }
    for (unsigned i = 0; i < trx_overlap; ++i) {
      trx--;
    }

    DagBlock blk(blk_hash_t(pivot),                              // pivot
                 level_t(0),                                     // level
                 {blk_hash_t(2), blk_hash_t(3), blk_hash_t(4)},  // tips
                 trxs,                                           // trxs
                 sig_t(7777),                                    // sig
                 blk_hash_t(hash),                               // hash
                 addr_t(12345));                                 // sender

    blks.emplace_back(blk);
  }
  return blks;
}

inline std::vector<std::pair<DagBlock, std::vector<std::shared_ptr<Transaction>>>>
createMockDagBlkSamplesWithSignedTransactions(unsigned pivot_start, unsigned blk_num, unsigned trx_start,
                                              unsigned trx_len, unsigned /*trx_overlap*/, secret_t const &sk) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<std::pair<DagBlock, std::vector<std::shared_ptr<Transaction>>>> blks;
  for (auto i = pivot_start; i < blk_num; ++i) {
    auto full_trx = createSignedTrxSamples(trx_start + i * trx_len, trx_len, sk);
    vec_trx_t trxs;
    for (auto t : full_trx) trxs.push_back(t->getHash());

    DagBlock blk(blk_hash_t(i),      // pivot
                 0,                  // level
                 {},                 // tips
                 trxs,               // trxs
                 sig_t(7777),        // sig
                 blk_hash_t(i + 1),  // hash
                 addr_t(12345));     // sender

    blks.emplace_back(std::make_pair(blk, full_trx));
  }
  return blks;
}

//
inline std::vector<DagBlock> createMockDag0(
    std::string genesis = "0000000000000000000000000000000000000000000000000000000000000000") {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk2(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk3(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk4(blk1.getHash(),  // pivot
                2,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk5(blk1.getHash(),    // pivot
                2,                 // level
                {blk2.getHash()},  // tips
                {}, secret_t::random());
  DagBlock blk6(blk3.getHash(),  // pivot
                2,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk7(blk5.getHash(),    // pivot
                3,                 // level
                {blk6.getHash()},  // tips
                {}, secret_t::random());
  DagBlock blk8(blk5.getHash(),  // pivot
                3,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk9(blk6.getHash(),  // pivot
                3,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk10(blk7.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk11(blk7.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk12(blk9.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk13(blk10.getHash(),  // pivot
                 5,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk14(blk11.getHash(),    // pivot
                 5,                  // level
                 {blk12.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk15(blk13.getHash(),    // pivot
                 6,                  // level
                 {blk14.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk16(blk13.getHash(),  // pivot
                 6,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk17(blk12.getHash(),  // pivot
                 5,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk18(blk15.getHash(),                                     // pivot
                 7,                                                   // level
                 {blk8.getHash(), blk16.getHash(), blk17.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk19(blk18.getHash(),  // pivot
                 8,                // level
                 {},               // tips
                 {}, secret_t::random());
  blks.emplace_back(dummy);
  blks.emplace_back(blk1);
  blks.emplace_back(blk2);
  blks.emplace_back(blk3);
  blks.emplace_back(blk4);
  blks.emplace_back(blk5);
  blks.emplace_back(blk6);
  blks.emplace_back(blk7);
  blks.emplace_back(blk8);
  blks.emplace_back(blk9);
  blks.emplace_back(blk10);
  blks.emplace_back(blk11);
  blks.emplace_back(blk12);
  blks.emplace_back(blk13);
  blks.emplace_back(blk14);
  blks.emplace_back(blk15);
  blks.emplace_back(blk16);
  blks.emplace_back(blk17);
  blks.emplace_back(blk18);
  blks.emplace_back(blk19);

  return blks;
}

//
inline std::vector<DagBlock> createMockDag1(
    std::string genesis = "0000000000000000000000000000000000000000000000000000000000000000") {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(1),        // hash
                addr_t(123));

  DagBlock blk2(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(2),        // hash
                addr_t(123));
  DagBlock blk3(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(3),        // hash
                addr_t(123));
  DagBlock blk4(blk_hash_t(1),  // pivot
                2,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(4),  // hash
                addr_t(123));
  DagBlock blk5(blk_hash_t(1),    // pivot
                2,                // level
                {blk_hash_t(2)},  // tips
                {},               // trxs
                sig_t(0),         // sig
                blk_hash_t(5),    // hash
                addr_t(123));
  DagBlock blk6(blk_hash_t(3),  // pivot
                2,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(6),  // hash
                addr_t(123));
  DagBlock blk7(blk_hash_t(5),    // pivot
                3,                // level
                {blk_hash_t(6)},  // tips
                {},               // trxs
                sig_t(0),         // sig
                blk_hash_t(7),    // hash
                addr_t(123));
  DagBlock blk8(blk_hash_t(5),  // pivot
                3,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(8),  // hash
                addr_t(123));
  DagBlock blk9(blk_hash_t(6),  // pivot
                3,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(9),  // hash
                addr_t(123));
  DagBlock blk10(blk_hash_t(7),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(10),  // hash
                 addr_t(123));
  DagBlock blk11(blk_hash_t(7),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(11),  // hash
                 addr_t(123));
  DagBlock blk12(blk_hash_t(9),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(12),  // hash
                 addr_t(123));
  DagBlock blk13(blk_hash_t(10),  // pivot
                 5,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(13),  // hash
                 addr_t(123));
  DagBlock blk14(blk_hash_t(11),    // pivot
                 5,                 // level
                 {blk_hash_t(12)},  // tips
                 {},                // trxs
                 sig_t(0),          // sig
                 blk_hash_t(14),    // hash
                 addr_t(123));
  DagBlock blk15(blk_hash_t(13),    // pivot
                 6,                 // level
                 {blk_hash_t(14)},  // tips
                 {},                // trxs
                 sig_t(0),          // sig
                 blk_hash_t(15),    // hash
                 addr_t(123));
  DagBlock blk16(blk_hash_t(13),  // pivot
                 6,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(16),  // hash
                 addr_t(123));
  DagBlock blk17(blk_hash_t(12),  // pivot
                 5,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(17),  // hash
                 addr_t(123));
  DagBlock blk18(blk_hash_t(15),                                   // pivot
                 7,                                                // level
                 {blk_hash_t(8), blk_hash_t(16), blk_hash_t(17)},  // tips
                 {},                                               // trxs
                 sig_t(0),                                         // sig
                 blk_hash_t(18),                                   // hash
                 addr_t(123));
  DagBlock blk19(blk_hash_t(18),  // pivot
                 8,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(19),  // hash
                 addr_t(123));
  blks.emplace_back(dummy);
  blks.emplace_back(blk1);
  blks.emplace_back(blk2);
  blks.emplace_back(blk3);
  blks.emplace_back(blk4);
  blks.emplace_back(blk5);
  blks.emplace_back(blk6);
  blks.emplace_back(blk7);
  blks.emplace_back(blk8);
  blks.emplace_back(blk9);
  blks.emplace_back(blk10);
  blks.emplace_back(blk11);
  blks.emplace_back(blk12);
  blks.emplace_back(blk13);
  blks.emplace_back(blk14);
  blks.emplace_back(blk15);
  blks.emplace_back(blk16);
  blks.emplace_back(blk17);
  blks.emplace_back(blk18);
  blks.emplace_back(blk19);

  return blks;
}

}  // namespace taraxa::core_tests::samples
