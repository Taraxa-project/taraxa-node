#pragma once

#include <vector>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::pbft {

/**
 * @brief Calculate DAG blocks ordering hash
 * @param dag_block_hashes DAG blocks hashes
 * @param trx_hashes transactions hashes
 * @return DAG blocks ordering hash
 */
blk_hash_t calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes,
                              const std::vector<trx_hash_t> &trx_hashes);

/**
 * @brief Calculate DAG blocks ordering hash
 * @param dag_blocks DAG blocks
 * @param transactions transactions
 * @return DAG blocks ordering hash
 */
blk_hash_t calculateOrderHash(const std::vector<DagBlock> &dag_blocks, const SharedTransactions &transactions);

}  // namespace taraxa::pbft
