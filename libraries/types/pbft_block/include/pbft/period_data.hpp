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
  PeriodData(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> const& cert_votes);
  explicit PeriodData(dev::RLP&& all_rlp);
  explicit PeriodData(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<Vote>> previous_block_cert_votes;  // These votes are the cert votes of previous block
                                                                 // which match reward votes in current pbft block
  size_t bonus_votes_count;

  std::vector<DagBlock> dag_blocks;
  SharedTransactions transactions;

  const static size_t kItemCount = 5;

  /**
   * @brief Recursive Length Prefix
   * @return bytes of RLP stream
   */
  bytes rlp() const;

  /**
   * @brief Clear PBFT block, certify votes, DAG blocks, and transactions
   */
  void clear();

  /**
   * @brief Verify there are enough valid certify votes
   * @param valid_sortition_players total DPOS votes count
   * @param sortition_threshold PBFT sortition threshold
   * @param pbft_2t_plus_1 PBFT 2t+1
   * @param dpos_eligible_vote_count function of getting voter DPOS eligible votes count
   */
  void hasEnoughValidCertVotes(const std::vector<std::shared_ptr<Vote>>& votes, size_t valid_sortition_players,
                               size_t sortition_threshold, size_t pbft_2t_plus_1,
                               std::function<size_t(addr_t const&)> const& dpos_eligible_vote_count) const;
};
std::ostream& operator<<(std::ostream& strm, PeriodData const& b);

/** @}*/

}  // namespace taraxa