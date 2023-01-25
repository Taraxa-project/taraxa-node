#include "test_util/samples.hpp"

namespace taraxa::core_tests::samples {
bool sendTrx(uint64_t count, unsigned port) {
  auto pattern = R"(
      curl --silent -m 10 --output /dev/null -d \
      '{
        "jsonrpc": "2.0",
        "method": "send_coin_transaction",
        "id": "0",
        "params": [
          {
            "nonce": %d,
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
    auto retcode = system(fmt(pattern, i + 1, val_t(TEST_TX_GAS_LIMIT), val_t(0), addr_t::random(),
                              samples::TX_GEN->getRandomUniqueSenderSecret().makeInsecure(), port)
                              .c_str());
    if (retcode != 0) {
      return false;
    }
  }
  return true;
}

SharedTransactions createSignedTrxSamples(unsigned start, unsigned num, secret_t const& sk, bytes data) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  SharedTransactions trxs;
  for (auto i = start; i <= num; ++i) {
    trxs.emplace_back(std::make_shared<Transaction>(i, i * 100, 0, 100000, data, sk, addr_t::random()));
  }
  return trxs;
}

std::vector<DagBlock> createMockDagBlkSamples(unsigned pivot_start, unsigned blk_num, unsigned trx_start,
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

std::vector<DagBlock> createMockDag0(const blk_hash_t& genesis) {
  std::vector<DagBlock> blks;
  DagBlock blk1(genesis,  // pivot
                1,        // level
                {},       // tips
                {}, secret_t::random());
  DagBlock blk2(genesis,  // pivot
                1,        // level
                {},       // tips
                {}, secret_t::random());
  DagBlock blk3(genesis,  // pivot
                1,        // level
                {},       // tips
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

std::vector<DagBlock> createMockDag1(const blk_hash_t& genesis) {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(genesis,        // pivot
                1,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(1),  // hash
                addr_t(123));

  DagBlock blk2(genesis,        // pivot
                1,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(2),  // hash
                addr_t(123));
  DagBlock blk3(genesis,        // pivot
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

}  // namespace taraxa::core_tests::samples