#pragma once

#include <unordered_map>

#include "common/types.hpp"

namespace taraxa {

enum class TwoTPlusOneVotedBlockType { SoftVotedBlock, CertVotedBlock, NextVotedBlock, NextVotedNullBlock };

struct VerifiedVotes {
  struct StepVotes {
    std::unordered_map<blk_hash_t, std::pair<uint64_t, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>>> votes;
    std::unordered_map<addr_t, std::pair<std::shared_ptr<Vote>, std::shared_ptr<Vote>>> unique_voters;
  };

  // 2t+1 voted blocks
  std::unordered_map<TwoTPlusOneVotedBlockType, std::pair<blk_hash_t, PbftStep>> two_t_plus_one_voted_blocks_;

  // Step votes
  std::map<PbftStep, StepVotes> step_votes;
};

}  // namespace taraxa