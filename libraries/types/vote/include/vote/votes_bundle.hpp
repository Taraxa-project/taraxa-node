#pragma once

#include "common/types.hpp"
#include "vote.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

/**
 * @brief VotesBundle stores >= 2t+1 votes with same voted hash, period, round and step. It is used for iptimized
 *        rlp storage so we dont save these duplicate data for each vote.
 */
class VotesBundle {
 public:
  VotesBundle(const std::vector<std::shared_ptr<Vote>>& votes, bool validate_common_data = true);
  // Ctor for VotesBundle syncing packets rlp
  VotesBundle(const dev::RLP& rlp);
  // Ctor for PeriodData rlp - it does not contain block_hash, period and step(certify) as those are already stored in
  // Pbft block rlp
  VotesBundle(const blk_hash_t& block_hash, PbftPeriod period, PbftRound round, PbftStep step, const dev::RLP& rlp);

  bytes rlp() const;

 private:
  // Common votes block hash
  blk_hash_t block_hash_;
  // Common votes period
  PbftPeriod period_{0};
  // Common votes round
  PbftRound round_{0};
  // Common votes step
  PbftStep step_{0};
  // >= 2t+1 votes
  std::vector<std::shared_ptr<Vote>> votes_;
};

/** @}*/

}  // namespace taraxa
