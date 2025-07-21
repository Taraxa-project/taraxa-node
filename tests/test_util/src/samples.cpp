#include "test_util/samples.hpp"

namespace taraxa::core_tests::samples {
SharedTransactions createSignedTrxSamples(unsigned start, unsigned num, secret_t const& sk, bytes data) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  SharedTransactions trxs;
  for (auto i = start; i <= num; ++i) {
    trxs.emplace_back(std::make_shared<Transaction>(i, i * 100, 1000000000, 100000, data, sk, addr_t::random()));
  }
  return trxs;
}

std::vector<std::shared_ptr<DagBlock>> createMockDag0(const blk_hash_t& genesis) {
  std::vector<std::shared_ptr<DagBlock>> blks;
  auto blk1 = std::make_shared<DagBlock>(genesis,      // pivot
                                         1,            // level
                                         vec_blk_t{},  // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk2 = std::make_shared<DagBlock>(genesis,      // pivot
                                         1,            // level
                                         vec_blk_t{},  // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk3 = std::make_shared<DagBlock>(genesis,      // pivot
                                         1,            // level
                                         vec_blk_t{},  // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk4 = std::make_shared<DagBlock>(blk1->getHash(),  // pivot
                                         2,                // level
                                         vec_blk_t{},      // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk5 = std::make_shared<DagBlock>(blk1->getHash(),             // pivot
                                         2,                           // level
                                         vec_blk_t{blk2->getHash()},  // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk6 = std::make_shared<DagBlock>(blk3->getHash(),  // pivot
                                         2,                // level
                                         vec_blk_t{},      // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk7 = std::make_shared<DagBlock>(blk5->getHash(),             // pivot
                                         3,                           // level
                                         vec_blk_t{blk6->getHash()},  // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk8 = std::make_shared<DagBlock>(blk5->getHash(),  // pivot
                                         3,                // level
                                         vec_blk_t{},      // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk9 = std::make_shared<DagBlock>(blk6->getHash(),  // pivot
                                         3,                // level
                                         vec_blk_t{},      // tips
                                         vec_trx_t{}, secret_t::random());
  auto blk10 = std::make_shared<DagBlock>(blk7->getHash(),  // pivot
                                          4,                // level
                                          vec_blk_t{},      // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk11 = std::make_shared<DagBlock>(blk7->getHash(),  // pivot
                                          4,                // level
                                          vec_blk_t{},      // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk12 = std::make_shared<DagBlock>(blk9->getHash(),  // pivot
                                          4,                // level
                                          vec_blk_t{},      // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk13 = std::make_shared<DagBlock>(blk10->getHash(),  // pivot
                                          5,                 // level
                                          vec_blk_t{},       // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk14 = std::make_shared<DagBlock>(blk11->getHash(),             // pivot
                                          5,                            // level
                                          vec_blk_t{blk12->getHash()},  // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk15 = std::make_shared<DagBlock>(blk13->getHash(),             // pivot
                                          6,                            // level
                                          vec_blk_t{blk14->getHash()},  // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk16 = std::make_shared<DagBlock>(blk13->getHash(),  // pivot
                                          6,                 // level
                                          vec_blk_t{},       // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk17 = std::make_shared<DagBlock>(blk12->getHash(),  // pivot
                                          5,                 // level
                                          vec_blk_t{},       // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk18 = std::make_shared<DagBlock>(blk15->getHash(),                                                // pivot
                                          7,                                                               // level
                                          vec_blk_t{blk8->getHash(), blk16->getHash(), blk17->getHash()},  // tips
                                          vec_trx_t{}, secret_t::random());
  auto blk19 = std::make_shared<DagBlock>(blk18->getHash(),  // pivot
                                          8,                 // level
                                          vec_blk_t{},       // tips
                                          vec_trx_t{}, secret_t::random());
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

std::vector<std::shared_ptr<DagBlock>> createMockDag1(const blk_hash_t& genesis) {
  std::vector<std::shared_ptr<DagBlock>> blks;
  std::shared_ptr<DagBlock> dummy;
  auto blk1 = std::make_shared<DagBlock>(genesis,        // pivot
                                         1,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(1),  // hash
                                         addr_t(123));

  auto blk2 = std::make_shared<DagBlock>(genesis,        // pivot
                                         1,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(2),  // hash
                                         addr_t(123));
  auto blk3 = std::make_shared<DagBlock>(genesis,        // pivot
                                         1,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(3),  // hash
                                         addr_t(123));
  auto blk4 = std::make_shared<DagBlock>(blk_hash_t(1),  // pivot
                                         2,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(4),  // hash
                                         addr_t(123));
  auto blk5 = std::make_shared<DagBlock>(blk_hash_t(1),             // pivot
                                         2,                         // level
                                         vec_blk_t{blk_hash_t(2)},  // tips
                                         vec_trx_t{},               // trxs
                                         sig_t(0),                  // sig
                                         blk_hash_t(5),             // hash
                                         addr_t(123));
  auto blk6 = std::make_shared<DagBlock>(blk_hash_t(3),  // pivot
                                         2,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(6),  // hash
                                         addr_t(123));
  auto blk7 = std::make_shared<DagBlock>(blk_hash_t(5),             // pivot
                                         3,                         // level
                                         vec_blk_t{blk_hash_t(6)},  // tips
                                         vec_trx_t{},               // trxs
                                         sig_t(0),                  // sig
                                         blk_hash_t(7),             // hash
                                         addr_t(123));
  auto blk8 = std::make_shared<DagBlock>(blk_hash_t(5),  // pivot
                                         3,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(8),  // hash
                                         addr_t(123));
  auto blk9 = std::make_shared<DagBlock>(blk_hash_t(6),  // pivot
                                         3,              // level
                                         vec_blk_t{},    // tips
                                         vec_trx_t{},    // trxs
                                         sig_t(0),       // sig
                                         blk_hash_t(9),  // hash
                                         addr_t(123));
  auto blk10 = std::make_shared<DagBlock>(blk_hash_t(7),   // pivot
                                          4,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(10),  // hash
                                          addr_t(123));
  auto blk11 = std::make_shared<DagBlock>(blk_hash_t(7),   // pivot
                                          4,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(11),  // hash
                                          addr_t(123));
  auto blk12 = std::make_shared<DagBlock>(blk_hash_t(9),   // pivot
                                          4,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(12),  // hash
                                          addr_t(123));
  auto blk13 = std::make_shared<DagBlock>(blk_hash_t(10),  // pivot
                                          5,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(13),  // hash
                                          addr_t(123));
  auto blk14 = std::make_shared<DagBlock>(blk_hash_t(11),             // pivot
                                          5,                          // level
                                          vec_blk_t{blk_hash_t(12)},  // tips
                                          vec_trx_t{},                // trxs
                                          sig_t(0),                   // sig
                                          blk_hash_t(14),             // hash
                                          addr_t(123));
  auto blk15 = std::make_shared<DagBlock>(blk_hash_t(13),             // pivot
                                          6,                          // level
                                          vec_blk_t{blk_hash_t(14)},  // tips
                                          vec_trx_t{},                // trxs
                                          sig_t(0),                   // sig
                                          blk_hash_t(15),             // hash
                                          addr_t(123));
  auto blk16 = std::make_shared<DagBlock>(blk_hash_t(13),  // pivot
                                          6,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(16),  // hash
                                          addr_t(123));
  auto blk17 = std::make_shared<DagBlock>(blk_hash_t(12),  // pivot
                                          5,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
                                          sig_t(0),        // sig
                                          blk_hash_t(17),  // hash
                                          addr_t(123));
  auto blk18 = std::make_shared<DagBlock>(blk_hash_t(15),                                            // pivot
                                          7,                                                         // level
                                          vec_blk_t{blk_hash_t(8), blk_hash_t(16), blk_hash_t(17)},  // tips
                                          vec_trx_t{},                                               // trxs
                                          sig_t(0),                                                  // sig
                                          blk_hash_t(18),                                            // hash
                                          addr_t(123));
  auto blk19 = std::make_shared<DagBlock>(blk_hash_t(18),  // pivot
                                          8,               // level
                                          vec_blk_t{},     // tips
                                          vec_trx_t{},     // trxs
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