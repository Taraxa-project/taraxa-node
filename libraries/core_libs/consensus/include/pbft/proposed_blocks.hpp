#pragma once

#include <shared_mutex>
#include <map>
#include <optional>
#include "common/types.hpp"

namespace taraxa {

class PbftBlock;
class Vote;

/**
 * @brief class ProposedBlocks holds proposed pbft blocks together with propose votes hashes per period & round
 */
class ProposedBlocks {
 public:
  /**
   * @brief Push proposed PBFT block in PBFT unverified queue
   * @param proposed_block proposed PBFT block
   * @param propose_vote propose PBFT vote
   * @return <true, ""> if block was successfully pushed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> pushProposedPbftBlock(const std::shared_ptr<PbftBlock>& proposed_block, const std::shared_ptr<Vote>& propose_vote);

  /**
   * @brief Get a proposed PBFT block and vote based on specified period, round and block hash
   * @param period
   * @param round
   * @param block_hash
   * @return proposed PBFT block
   */
  std::shared_ptr<PbftBlock> getPbftProposedBlock(uint64_t period, uint64_t round, const blk_hash_t& block_hash) const;

  /**
   * @brief Cleanup all proposed PBFT blocks for the finalized period
   * @param period
   */
  void cleanupProposedPbftBlocksByPeriod(uint64_t period);

  /**
   * @brief Cleanup all proposed PBFT blocks for the finalized round - 1. We must keep previous round proposed blocks
   *        for voting purposes
   * @param period
   * @param round
   */
  void cleanupProposedPbftBlocksByRound(uint64_t period, uint64_t round);

 private:

  // <PBFT period, <PBFT round, <block hash, pair<propose vote hash, propose block>>>
  std::map<uint64_t,
           std::map<uint64_t,
                    std::unordered_map<blk_hash_t,
                                         std::pair<vote_hash_t, std::shared_ptr<PbftBlock>>>>>
      proposed_blocks_;
  mutable std::shared_mutex proposed_blocks_mutex_;
};

}  // namespace taraxa