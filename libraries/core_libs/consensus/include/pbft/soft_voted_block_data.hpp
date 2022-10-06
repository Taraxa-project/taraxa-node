#pragma once

#include <libdevcore/RLP.h>

#include <memory>
#include <vector>

#include "common/types.hpp"

namespace taraxa {

class PbftBlock;
class Vote;

/**
 * @brief TwoTPlusOneSoftVotedBlockData holds data related to observed 2t+1 soft voted block
 */
class TwoTPlusOneSoftVotedBlockData {
 public:
  TwoTPlusOneSoftVotedBlockData() = default;
  TwoTPlusOneSoftVotedBlockData(const TwoTPlusOneSoftVotedBlockData&) = default;
  TwoTPlusOneSoftVotedBlockData& operator=(const TwoTPlusOneSoftVotedBlockData&) = default;
  TwoTPlusOneSoftVotedBlockData(TwoTPlusOneSoftVotedBlockData&&) = default;
  TwoTPlusOneSoftVotedBlockData& operator=(TwoTPlusOneSoftVotedBlockData&&) = default;

  TwoTPlusOneSoftVotedBlockData(dev::RLP const& rlp);
  bytes rlp() const;

  uint64_t round_;
  blk_hash_t block_hash_;
  std::vector<std::shared_ptr<Vote>> soft_votes_;
  // can be nullptr as node might not have the actual block even though it saw 2t+1 soft votes
  std::shared_ptr<PbftBlock> block_;

  static constexpr size_t kStandardDataItemCount{3};
  static constexpr size_t kExtendedDataItemCount{4};  // with block
};

}  // namespace taraxa
