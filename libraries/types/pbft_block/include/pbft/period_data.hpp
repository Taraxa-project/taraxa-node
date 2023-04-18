#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class Vote;
class PbftBlock;

/**
 * @brief PeriodData class is for block execution, that includes PBFT block, certify votes, DAG blocks, and transactions
 */
class PeriodData {
 public:
  PeriodData() = default;
  PeriodData(std::shared_ptr<PbftBlock> pbft_blk, const std::vector<std::shared_ptr<Vote>>& previous_block_cert_votes);
  explicit PeriodData(dev::RLP&& all_rlp);
  explicit PeriodData(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<Vote>> previous_block_cert_votes;  // These votes are the cert votes of previous block
                                                                 // which match reward votes in current pbft block
  std::vector<DagBlock> dag_blocks;
  SharedTransactions transactions;

  const static size_t kRlpItemCount = 4;

  /**
   * @brief Recursive Length Prefix
   * @return bytes of RLP stream
   */
  bytes rlp() const;

  /**
   * @brief Clear PBFT block, certify votes, DAG blocks, and transactions
   */
  void clear();
};
std::ostream& operator<<(std::ostream& strm, PeriodData const& b);

/** @}*/

}  // namespace taraxa