/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 14:03:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 16:19:10
 */

#ifndef CREATE_SAMPLES_HPP
#define CREATE_SAMPLES_HPP
#include <string>
#include "dag_block.hpp"
#include "transaction.hpp"

namespace taraxa {
namespace samples {

std::vector<Transaction> createTrxSamples(unsigned start, unsigned num) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  std::vector<Transaction> trxs;
  for (auto i = start; i < num; ++i) {
    std::stringstream strm;
    strm << std::setw(64) << std::setfill('0');
    strm << std::to_string(i);
    std::string hash = strm.str();
    Transaction trx(hash,                     // hash
                    Transaction::Type::Null,  // type
                    2,                        // nonce
                    3,                        // value
                    "4000000000000000000000000000000000000000000000000000000000"
                    "000004",  // gas_price
                    "5000000000000000000000000000000000000000000000000000000000"
                    "000005",  // gas
                    hash,      // receiver
                    "7777777777777777777777777777777777777777777777777777777777"
                    "7777777777777777777777777777777777777777777777777777777777"
                    "777777777777",  // sig
                    str2bytes("00FEDCBA9876543210000000"));
    trxs.emplace_back(trx);
  }
  return trxs;
}

std::vector<DagBlock> createDagBlkSamples(unsigned pivot_start,
                                          unsigned blk_num, unsigned trx_start,
                                          unsigned trx_len,
                                          unsigned trx_overlap) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<DagBlock> blks;
  unsigned trx = trx_start;
  for (auto i = pivot_start; i < blk_num; ++i) {
    std::string pivot;
    vec_trx_t trxs;
    {
      std::stringstream strm;
      strm << std::setw(64) << std::setfill('0');
      strm << std::to_string(i);
      pivot = strm.str();
    }
    // create overlapped trxs
    {
      for (auto i = 0; i < trx_len; ++i, trx++) {
        std::stringstream strm;
        strm << std::setw(64) << std::setfill('0');
        strm << std::to_string(trx);
        std::string trx = strm.str();
        trxs.emplace_back(trx);
      }
      for (auto i = 0; i < trx_overlap; ++i) {
        trx--;
      }
    }

    DagBlock blk(
        pivot,  // pivot
        {"222222222222222222222222222222222222222222222222222222222222"
         "2222",
         "333333333333333333333333333333333333333333333333333333333333"
         "3333",
         "444444444444444444444444444444444444444444444444444444444444"
         "4444"},  // tips
        trxs,      // trxs
        "7777777777777777777777777777777777777777777777777777777777777"
        "777777777"
        "77"
        "77777777777777777777777777777777777777777777777777777777",  // sig
        pivot,                                                       // hash
        "0000000000000000000000000000000000000000000000000000000000000"
        "00F");  // publisher

    blks.emplace_back(blk);
  }
  return blks;
}

}  // namespace samples
}  // namespace taraxa

#endif
