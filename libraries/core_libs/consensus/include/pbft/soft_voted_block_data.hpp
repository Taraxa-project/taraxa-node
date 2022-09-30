#pragma once

#include <memory>
#include <vector>

namespace taraxa {

class PbftBlock;
class Vote;

/**
 * @brief TwoTPlusOneSoftVotedBlockData holds data related to observed 2t+1 soft voted block
 */
struct TwoTPlusOneSoftVotedBlockData {
  uint64_t round;
  blk_hash_t block_hash;
  std::vector<std::shared_ptr<Vote>> soft_votes;
  // can be nullptr as node might not have the actual block even though it saw 2t+1 soft votes
  std::shared_ptr<PbftBlock> block;

  static constexpr size_t kStandardDataItemCount{3};
  static constexpr size_t kExtendedDataItemCount{4};  // with block
};

}  // namespace taraxa
