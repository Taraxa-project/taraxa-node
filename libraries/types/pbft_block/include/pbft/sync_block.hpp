#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/types.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class Vote;
class PbftBlock;
class DagBlock;

/**
 * @brief SyncBlock class is for block execution, that includes PBFT block, certify votes, DAG blocks, and transactions
 */
class SyncBlock {
 public:
  SyncBlock() = default;
  SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> const& cert_votes);
  explicit SyncBlock(dev::RLP&& all_rlp);
  explicit SyncBlock(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<Vote>> cert_votes;
  std::vector<DagBlock> dag_blocks;
  SharedTransactions transactions;

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
  void hasEnoughValidCertVotes(size_t valid_sortition_players, size_t sortition_threshold, size_t pbft_2t_plus_1,
                               std::function<size_t(addr_t const&)> const& dpos_eligible_vote_count) const;
};
std::ostream& operator<<(std::ostream& strm, SyncBlock const& b);

/** @}*/

}  // namespace taraxa