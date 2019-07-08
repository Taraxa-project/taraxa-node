/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 14:03:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 16:19:10
 */

#ifndef CREATE_SAMPLES_HPP
#define CREATE_SAMPLES_HPP
#include <executor.hpp>
#include <fstream>
#include <map>
#include <string>
#include "dag_block.hpp"
#include "transaction.hpp"

namespace taraxa::samples {

inline const val_t::Arith TEST_MAX_TRANSACTIONS_IN_BLOCK = 10000;
inline const val_t::Arith TEST_BLOCK_GAS_LIMIT = Executor::MOCK_BLOCK_GAS_LIMIT;
inline const val_t::Arith TEST_TX_GAS_LIMIT =
    TEST_BLOCK_GAS_LIMIT / TEST_MAX_TRANSACTIONS_IN_BLOCK;

class TxGenerator {
  mutable std::mutex mutex;
  mutable std::unordered_set<dev::FixedHash<secret_t::size>> usedSecrets;

 public:
  // this function guarantees uniqueness for generated values
  // scoped to the class instance
  auto getRandomUniqueSenderSecret() const {
    std::unique_lock l(mutex);
    auto secret = secret_t::random();
    while (!usedSecrets.insert(secret.makeInsecure()).second) {
      secret = secret_t::random();
    }
    return secret;
  }

  auto getWithRandomUniqueSender(
      const bal_t &value = 0, const addr_t &to = addr_t::random(),
      const bytes &data = str2bytes("00FEDCBA9876543210000000")) const {
    return Transaction(0,                               // nonce
                       value,                           // value
                       val_t(0),                        // gas_price
                       TEST_TX_GAS_LIMIT,               // gas
                       to,                              // receiver
                       data,                            // data
                       getRandomUniqueSenderSecret());  // secret
  }
};

inline const TxGenerator TX_GEN;

struct TestAccount {
  TestAccount() = default;
  TestAccount(int id, std::string const &sk, std::string const &pk,
              std::string const &addr)
      : id(id), sk(sk), pk(pk), addr(addr) {}
  friend std::ostream;
  int id;
  std::string sk;
  std::string pk;
  std::string addr;
};

inline std::ostream &operator<<(std::ostream &strm, TestAccount const &acc) {
  strm << "sk: " << acc.sk << "\npk: " << acc.pk << "\naddr: " << acc.addr
       << std::endl;
  return strm;
}

inline std::map<int, TestAccount> createTestAccountTable(
    std::string const &filename) {
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

inline std::vector<Transaction> createMockTrxSamples(unsigned start,
                                                     unsigned num) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  std::vector<Transaction> trxs;
  for (auto i = start; i < num; ++i) {
    std::stringstream strm;
    strm << std::setw(64) << std::setfill('0');
    strm << std::to_string(i);
    std::string hash = strm.str();
    Transaction trx(trx_hash_t(hash),         // hash
                    Transaction::Type::Call,  // type
                    2,                        // nonce
                    3,                        // value
                    val_t(4),                 // gas_price
                    val_t(5),                 // gas
                    addr_t(i * 1000),         // receiver
                    sig_t(),                  // sig
                    str2bytes("00FEDCBA9876543210000000"));
    trxs.emplace_back(trx);
  }
  return trxs;
}

inline std::vector<Transaction> createSignedTrxSamples(unsigned start,
                                                       unsigned num,
                                                       secret_t const &sk) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  std::vector<Transaction> trxs;
  for (auto i = start; i < num; ++i) {
    std::stringstream strm;
    strm << std::setw(64) << std::setfill('0');
    strm << std::to_string(i);
    std::string hash = strm.str();
    Transaction trx(i,               // nonce
                    i * 100,         // value
                    val_t(0),        // gas_price
                    val_t(1000000),  // gas
                    // (i + 1) - because zero address is reserved
                    addr_t((i + 1) * 100),                  // receiver
                    str2bytes("00FEDCBA9876543210000000"),  // data
                    sk);                                    // secret
    trxs.emplace_back(trx);
  }
  return trxs;
}

inline std::vector<DagBlock> createMockDagBlkSamples(unsigned pivot_start,
                                                     unsigned blk_num,
                                                     unsigned trx_start,
                                                     unsigned trx_len,
                                                     unsigned trx_overlap) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<DagBlock> blks;
  unsigned trx = trx_start;
  for (auto i = pivot_start; i < blk_num; ++i) {
    std::string pivot, hash;

    vec_trx_t trxs;
    {
      std::stringstream strm;
      strm << std::setw(64) << std::setfill('0');
      strm << std::to_string(i);
      pivot = strm.str();
    }
    {
      std::stringstream strm;
      strm << std::setw(64) << std::setfill('0');
      strm << std::to_string(i + 1);
      hash = strm.str();
    }
    // create overlapped trxs
    {
      for (auto i = 0; i < trx_len; ++i, trx++) {
        std::stringstream strm;
        strm << std::setw(64) << std::setfill('0');
        strm << std::to_string(trx);
        std::string trx = strm.str();
        trxs.emplace_back(trx_hash_t(trx));
      }
      for (auto i = 0; i < trx_overlap; ++i) {
        trx--;
      }
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

inline std::vector<std::pair<DagBlock, std::vector<Transaction>>>
createMockDagBlkSamplesWithSignedTransactions(
    unsigned pivot_start, unsigned blk_num, unsigned trx_start,
    unsigned trx_len, unsigned trx_overlap, secret_t const &sk) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<std::pair<DagBlock, std::vector<Transaction>>> blks;
  unsigned trx = trx_start;
  for (auto i = pivot_start; i < blk_num; ++i) {
    std::string pivot, hash;
    auto full_trx =
        createSignedTrxSamples(trx_start + i * trx_len, trx_len, sk);
    vec_trx_t trxs;
    for (auto t : full_trx) trxs.push_back(t.getHash());
    {
      std::stringstream strm;
      strm << std::setw(64) << std::setfill('0');
      strm << std::to_string(i);
      pivot = strm.str();
    }
    {
      std::stringstream strm;
      strm << std::setw(64) << std::setfill('0');
      strm << std::to_string(i + 1);
      hash = strm.str();
    }

    DagBlock blk(blk_hash_t(pivot),  // pivot
                 0,                  // level
                 {},                 // tips
                 trxs,               // trxs
                 sig_t(7777),        // sig
                 blk_hash_t(hash),   // hash
                 addr_t(12345));     // sender

    blks.emplace_back(std::make_pair(blk, full_trx));
  }
  return blks;
}

//
inline std::vector<DagBlock> createMockDag0() {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(blk_hash_t(0),  // pivot
                1,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(1),  // hash
                addr_t(123));

  DagBlock blk2(blk_hash_t(0),  // pivot
                1,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(2),  // hash
                addr_t(123));
  DagBlock blk3(blk_hash_t(0),  // pivot
                1,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(3),  // hash
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

}  // namespace taraxa::samples

#endif
