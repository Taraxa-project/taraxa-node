#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class PbftVote;
class PbftBlock;
class PillarVote;

/**
 * @brief PeriodData class is for block execution, that includes PBFT block, certify votes, DAG blocks, and transactions
 */
class PeriodData {
 public:
  PeriodData() = default;
  PeriodData(std::shared_ptr<PbftBlock> pbft_blk,
             const std::vector<std::shared_ptr<PbftVote>>& previous_block_cert_votes,
             std::optional<std::vector<std::shared_ptr<PillarVote>>>&& pillar_votes = {});
  explicit PeriodData(const dev::RLP& all_rlp);
  explicit PeriodData(const bytes& all_rlp);

  static PeriodData FromOldPeriodData(const dev::RLP& rlp);
  static bytes ToOldPeriodData(const bytes& rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<PbftVote>> previous_block_cert_votes;  // These votes are the cert votes of previous block
                                                                     // which match reward votes in current pbft block
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  SharedTransactions transactions;

  // Pillar votes should be present only if pbft block contains also pillar block hash
  std::optional<std::vector<std::shared_ptr<PillarVote>>> pillar_votes_;

  // Period data rlp without pillar votes
  constexpr static size_t kBaseRlpItemCount = 4;

  // Period data rlp with pillar votes (optional since ficus hardfork)
  constexpr static size_t kExtendedRlpItemCount = 5;

  /**
   * @brief Recursive Length Prefix
   * @return bytes of RLP stream
   */
  bytes rlp() const;

  /**
   * @brief Clear PBFT block, certify votes, DAG blocks, and transactions
   */
  void clear();

  HAS_RLP_FIELDS
};
std::ostream& operator<<(std::ostream& strm, PeriodData const& b);

/** @}*/

}  // namespace taraxa