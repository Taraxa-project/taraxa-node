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

  // Greatest step, for which there is at least t+1 next votes - it is used for lambda exponential backoff: Usually
  // when network gets stalled it is due to lack of 2t+1 voting power and steps keep increasing. When new node joins
  // the network, it should catch up with the rest of nodes asap so we dont start exponentially backing of its lambda
  // if it's current step is far behind network_t_plus_one_step (at least 1 third of network is at this step)
  PbftStep network_t_plus_one_step{0};
};

}  // namespace taraxa