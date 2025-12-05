#pragma once

#include <shared_mutex>
#include <unordered_map>

#include "common/types.hpp"
#include "logger/logger.hpp"

namespace taraxa {

class PbftVote;

enum class TwoTPlusOneVotedBlockType { SoftVotedBlock, CertVotedBlock, NextVotedBlock, NextVotedNullBlock };

struct VotedBlock {
  blk_hash_t hash;
  PbftStep step;
};

using TwoTVotedBlockMap = std::unordered_map<TwoTPlusOneVotedBlockType, VotedBlock>;

struct VotesWithWeight {
  uint64_t weight;
  std::unordered_map<vote_hash_t, std::shared_ptr<PbftVote>> votes;
};

using UniqueVotersMap = std::unordered_map<addr_t, std::pair<std::shared_ptr<PbftVote>, std::shared_ptr<PbftVote>>>;

struct StepVotes {
  std::unordered_map<blk_hash_t, VotesWithWeight> votes;
  UniqueVotersMap unique_voters;
};
using StepVotesMap = std::map<PbftStep, StepVotes>;

struct RoundVerifiedVotes {
  // 2t+1 voted blocks
  TwoTVotedBlockMap two_t_plus_one_voted_blocks_;
  // Step votes
  StepVotesMap step_votes;

  // Greatest step, for which there is at least t+1 next votes - it is used for lambda exponential backoff: Usually
  // when network gets stalled it is due to lack of 2t+1 voting power and steps keep increasing. When new node joins
  // the network, it should catch up with the rest of nodes asap so we dont start exponentially backing of its lambda
  // if it's current step is far behind network_t_plus_one_step (at least 1 third of network is at this step)
  PbftStep network_t_plus_one_step{0};
};

using RoundVerifiedVotesMap = std::map<PbftRound, RoundVerifiedVotes>;
using PeriodVerifiedVotesMap = std::map<PbftPeriod, RoundVerifiedVotesMap>;

class VerifiedVotes {
 public:
  VerifiedVotes(addr_t node_addr) { LOG_OBJECTS_CREATE("VERIFIED_VOTES"); }

  uint64_t size() const;
  std::vector<std::shared_ptr<PbftVote>> votes() const;

  std::optional<const RoundVerifiedVotesMap> getPeriodVotes(PbftPeriod period) const;
  std::optional<const RoundVerifiedVotes> getRoundVotes(PbftPeriod period, PbftRound round) const;
  std::optional<const StepVotes> getStepVotes(PbftPeriod period, PbftRound round, PbftStep step) const;

  std::optional<VotedBlock> getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                     TwoTPlusOneVotedBlockType type) const;
  std::vector<std::shared_ptr<PbftVote>> getTwoTPlusOneVotedBlockVotes(PbftPeriod period, PbftRound round,
                                                                       TwoTPlusOneVotedBlockType type) const;
  void cleanupVotesByPeriod(PbftPeriod pbft_period);

  std::optional<std::shared_ptr<PbftVote>> insertUniqueVoter(const std::shared_ptr<PbftVote>& vote);

  std::optional<VotesWithWeight> insertVotedValue(const std::shared_ptr<PbftVote>& vote);

  void setNetworkTPlusOneStep(std::shared_ptr<PbftVote> vote);
  // Insert new 2t+1 voted block
  void insertTwoTPlusOneVotedBlock(TwoTPlusOneVotedBlockType type, std::shared_ptr<PbftVote> vote);

 private:
  std::pair<bool, PeriodVerifiedVotesMap::iterator> get(PbftPeriod period);
  std::pair<bool, PeriodVerifiedVotesMap::const_iterator> get(PbftPeriod period) const;
  std::pair<bool, RoundVerifiedVotesMap::iterator> get(PbftPeriod period, PbftRound round);
  std::pair<bool, RoundVerifiedVotesMap::const_iterator> get(PbftPeriod period, PbftRound round) const;
  std::pair<bool, StepVotesMap::iterator> get(PbftPeriod period, PbftRound round, PbftStep step);
  std::pair<bool, StepVotesMap::const_iterator> get(PbftPeriod period, PbftRound round, PbftStep step) const;

  mutable std::shared_mutex verified_votes_access_;
  PeriodVerifiedVotesMap verified_votes_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa