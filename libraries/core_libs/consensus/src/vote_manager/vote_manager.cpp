#include "vote_manager/vote_manager.hpp"

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <optional>
#include <shared_mutex>

#include "network/network.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa {

VoteManager::VoteManager(const FullNodeConfig& config, std::shared_ptr<DbStorage> db,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<final_chain::FinalChain> final_chain,
                         std::shared_ptr<KeyManager> key_manager, std::shared_ptr<SlashingManager> slashing_manager)
    : kPbftConfig(config.genesis.pbft),
      db_(std::move(db)),
      pbft_chain_(std::move(pbft_chain)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      slashing_manager_(std::move(slashing_manager)),
      verified_votes_(dev::toAddress(config.getFirstWallet().node_secret)),
      already_validated_votes_(1000000, 1000) {
  // Use first wallet as default node_addr
  const auto& node_addr = dev::toAddress(config.getFirstWallet().node_secret);
  LOG_OBJECTS_CREATE("VOTE_MGR");

  auto addVerifiedVotes = [this](const std::vector<std::shared_ptr<PbftVote>>& votes,
                                 bool set_reward_votes_info = false) {
    bool rewards_info_already_set = false;
    for (const auto& vote : votes) {
      // Check if votes are unique per round, step & voter
      if (!isUniqueVote(vote).first) {
        continue;
      }

      if (set_reward_votes_info && vote->getType() == PbftVoteTypes::cert_vote) {
        if (!rewards_info_already_set) {
          rewards_info_already_set = true;
          reward_votes_block_hash_ = vote->getBlockHash();
          reward_votes_period_ = vote->getPeriod();
          reward_votes_round_ = vote->getRound();
        } else {
          assert(reward_votes_block_hash_ == vote->getBlockHash());
          assert(reward_votes_period_ == vote->getPeriod());
          assert(reward_votes_round_ == vote->getRound());
        }
      }

      addVerifiedVote(vote);
      LOG(log_dg_) << "Vote " << vote->getHash() << " loaded from db to memory";
    }
  };

  // Load 2t+1 vote blocks votes
  addVerifiedVotes(db_->getAllTwoTPlusOneVotes(), true);

  // Load own votes
  const auto own_votes = db_->getOwnVerifiedVotes();
  for (const auto& own_vote : own_votes) {
    own_verified_votes_.emplace_back(own_vote);
  }
  addVerifiedVotes(own_votes);

  // Load reward votes
  const auto reward_votes = db_->getRewardVotes();
  for (const auto& reward_vote : reward_votes) {
    extra_reward_votes_.emplace_back(reward_vote->getHash());
  }
  addVerifiedVotes(reward_votes);
}

void VoteManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

std::vector<std::shared_ptr<PbftVote>> VoteManager::getVerifiedVotes() const { return verified_votes_.votes(); }

uint64_t VoteManager::getVerifiedVotesSize() const { return verified_votes_.size(); }

void VoteManager::cleanupVotesByPeriod(PbftPeriod pbft_period) { verified_votes_.cleanupVotesByPeriod(pbft_period); }

void VoteManager::setCurrentPbftPeriodAndRound(PbftPeriod pbft_period, PbftRound pbft_round) {
  current_pbft_period_ = pbft_period;
  current_pbft_round_ = pbft_round;

  auto round_votes = verified_votes_.getRoundVotes(pbft_period, pbft_round);
  if (!round_votes) {
    return;
  }

  // Check if we already have 2t+1 votes bundles for specified pbft period & round. If so, save those votes into db
  // During normal node operation this should happen rarely - it can happen only if we receive 2t+1 future votes for
  // a period or round that we are not yet in
  for (const auto& two_t_plus_one_voted_block : round_votes->two_t_plus_one_voted_blocks_) {
    const TwoTPlusOneVotedBlockType two_t_plus_one_voted_block_type = two_t_plus_one_voted_block.first;
    // 2t+1 cert voted blocks are only saved to the database in a db batch when block is pushed to the chain
    if (two_t_plus_one_voted_block_type != TwoTPlusOneVotedBlockType::CertVotedBlock) {
      const auto& [two_t_plus_one_voted_block_hash, two_t_plus_one_voted_block_step] =
          two_t_plus_one_voted_block.second;

      const auto found_step_votes_it = round_votes->step_votes.find(two_t_plus_one_voted_block_step);
      if (found_step_votes_it == round_votes->step_votes.end()) {
        LOG(log_er_) << "Unable to find 2t+1 votes in verified_votes for period " << pbft_period << ", round "
                     << pbft_round << ", step " << two_t_plus_one_voted_block_step;
        assert(false);
        return;
      }

      // Find verified votes for specified block_hash based on found 2t+1 voted block of type "type"
      const auto found_verified_votes_it = found_step_votes_it->second.votes.find(two_t_plus_one_voted_block_hash);
      if (found_verified_votes_it == found_step_votes_it->second.votes.end()) {
        LOG(log_er_) << "Unable to find 2t+1 votes in verified_votes for period " << pbft_period << ", round "
                     << pbft_round << ", step " << two_t_plus_one_voted_block_step << ", block hash "
                     << two_t_plus_one_voted_block_hash;
        assert(false);
        return;
      }

      std::vector<std::shared_ptr<PbftVote>> votes;
      votes.reserve(found_verified_votes_it->second.votes.size());
      for (const auto& vote : found_verified_votes_it->second.votes) {
        votes.push_back(vote.second);
      }

      db_->replaceTwoTPlusOneVotes(two_t_plus_one_voted_block_type, votes);
    }
  }
}

PbftStep VoteManager::getNetworkTplusOneNextVotingStep(PbftPeriod period, PbftRound round) const {
  auto round_votes = verified_votes_.getRoundVotes(period, round);
  if (!round_votes) {
    return 0;
  }

  return round_votes->network_t_plus_one_step;
}

bool VoteManager::addVerifiedVote(const std::shared_ptr<PbftVote>& vote) {
  assert(vote->getWeight().has_value());
  const auto hash = vote->getHash();
  const auto weight = *vote->getWeight();
  if (!weight) {
    LOG(log_er_) << "Unable to add vote " << hash << " into the verified queue. Invalid vote weight";
    return false;
  }

  const auto vote_block_hash = vote->getBlockHash();

  {
    if (auto existing_vote = verified_votes_.insertUniqueVoter(vote); existing_vote) {
      LOG(log_wr_) << "Non unique vote " << vote->getHash().abridged() << " (race condition)";
      // Create double voting proof
      slashing_manager_->submitDoubleVotingProof(vote, *existing_vote);
      return false;
    }

    // Old vote, ignore unless it is a reward vote
    bool is_valid_potential_reward_vote = false;
    if (vote->getPeriod() < current_pbft_period_) {
      if (is_valid_potential_reward_vote = isValidRewardVote(vote); !is_valid_potential_reward_vote) {
        LOG(log_tr_) << "Old vote " << vote->getHash().abridged() << " vote period" << vote->getPeriod()
                     << " current period " << current_pbft_period_;
        return false;
      }
    }

    const auto votes_with_weight = verified_votes_.insertVotedValue(vote);
    if (!votes_with_weight) {
      return false;
    }

    LOG(log_nf_) << "Added verified vote: " << hash;
    LOG(log_dg_) << "Added verified vote: " << *vote;

    if (is_valid_potential_reward_vote) {
      extra_reward_votes_.emplace_back(vote->getHash());
      db_->saveExtraRewardVote(vote);
    }

    // Unable to get 2t+1
    const auto two_t_plus_one = getPbftTwoTPlusOne(vote->getPeriod() - 1, vote->getType());
    if (!two_t_plus_one.has_value()) [[unlikely]] {
      LOG(log_er_) << "Cannot set(or not) 2t+1 voted block as 2t+1 threshold is unavailable, vote " << vote->getHash();
      return true;
    }

    auto round_votes = verified_votes_.getRoundVotes(vote->getPeriod(), vote->getRound());
    if (!round_votes) {
      return true;
    }

    const auto total_weight = votes_with_weight->weight;
    // Calculate t+1
    const auto t_plus_one = ((*two_t_plus_one - 1) / 2) + 1;
    // Set network_t_plus_one_step - used for triggering exponential backoff
    if (vote->getType() == PbftVoteTypes::next_vote && total_weight >= t_plus_one &&
        vote->getStep() > round_votes->network_t_plus_one_step) {
      verified_votes_.setNetworkTPlusOneStep(vote);
      LOG(log_nf_) << "Set t+1 next voted block " << vote->getHash() << " for period " << vote->getPeriod()
                   << ", round " << vote->getRound() << ", step " << vote->getStep();
    }

    // Not enough votes - do not set 2t+1 voted block for period,round and step
    if (total_weight < *two_t_plus_one) {
      return true;
    }

    // Function to save 2t+1 voted block + its votes
    auto saveTwoTPlusOneVotesInDb = [this, &round_votes, &votes_with_weight](
                                        TwoTPlusOneVotedBlockType two_plus_one_voted_block_type,
                                        const std::shared_ptr<PbftVote> vote) {
      auto found_two_t_plus_one_voted_block =
          round_votes->two_t_plus_one_voted_blocks_.find(two_plus_one_voted_block_type);

      // 2t+1 votes block already set
      if (found_two_t_plus_one_voted_block != round_votes->two_t_plus_one_voted_blocks_.end()) {
        assert(found_two_t_plus_one_voted_block->second.hash == vote->getBlockHash());

        // It is possible to have 2t+1 next votes for the same block in multiple steps
        if (two_plus_one_voted_block_type != TwoTPlusOneVotedBlockType::NextVotedBlock &&
            two_plus_one_voted_block_type != TwoTPlusOneVotedBlockType::NextVotedNullBlock) {
          assert(found_two_t_plus_one_voted_block->second.step == vote->getStep());
        }

        return;
      }

      // Insert new 2t+1 voted block
      verified_votes_.insertTwoTPlusOneVotedBlock(two_plus_one_voted_block_type, vote);

      // Save only current pbft period & round 2t+1 votes bundles into db
      // Cert votes are saved once the pbft block is pushed in the chain
      if (vote->getType() != PbftVoteTypes::cert_vote && vote->getPeriod() == current_pbft_period_ &&
          vote->getRound() == current_pbft_round_) {
        std::vector<std::shared_ptr<PbftVote>> votes;
        votes.reserve(votes_with_weight->votes.size());
        for (const auto& tmp_vote : votes_with_weight->votes) {
          votes.push_back(tmp_vote.second);
        }

        db_->replaceTwoTPlusOneVotes(two_plus_one_voted_block_type, votes);
      }
    };

    switch (vote->getType()) {
      case PbftVoteTypes::soft_vote:
        saveTwoTPlusOneVotesInDb(TwoTPlusOneVotedBlockType::SoftVotedBlock, vote);
        break;
      case PbftVoteTypes::cert_vote:
        saveTwoTPlusOneVotesInDb(TwoTPlusOneVotedBlockType::CertVotedBlock, vote);
        break;
      case PbftVoteTypes::next_vote:
        if (vote_block_hash == kNullBlockHash) {
          saveTwoTPlusOneVotesInDb(TwoTPlusOneVotedBlockType::NextVotedNullBlock, vote);
        } else {
          saveTwoTPlusOneVotesInDb(TwoTPlusOneVotedBlockType::NextVotedBlock, vote);
        }
        break;
      default:
        break;
    }
  }

  return true;
}

bool VoteManager::voteInVerifiedMap(const std::shared_ptr<PbftVote>& vote) const {
  const auto step_votes_map = verified_votes_.getStepVotes(vote->getPeriod(), vote->getRound(), vote->getStep());
  if (!step_votes_map) {
    return false;
  }

  const auto found_voted_value_it = step_votes_map->votes.find(vote->getBlockHash());
  if (found_voted_value_it == step_votes_map->votes.end()) {
    return false;
  }

  return found_voted_value_it->second.votes.find(vote->getHash()) != found_voted_value_it->second.votes.end();
}

std::pair<bool, std::shared_ptr<PbftVote>> VoteManager::isUniqueVote(const std::shared_ptr<PbftVote>& vote) const {
  const auto step_votes_map = verified_votes_.getStepVotes(vote->getPeriod(), vote->getRound(), vote->getStep());
  if (!step_votes_map) {
    return {true, nullptr};
  }

  const auto found_voter_it = step_votes_map->unique_voters.find(vote->getVoterAddr());
  if (found_voter_it == step_votes_map->unique_voters.end()) {
    return {true, nullptr};
  }

  if (found_voter_it->second.first->getHash() == vote->getHash()) {
    return {true, nullptr};
  }

  // Next votes (second finishing steps) are special case, where we allow voting for both kNullBlockHash and
  // some other specific block hash at the same time -> 2 unique votes per round & step & voter
  if (vote->getType() == PbftVoteTypes::next_vote && vote->getStep() % 2) {
    // New second next vote
    if (found_voter_it->second.second == nullptr) {
      // One of the next votes == kNullBlockHash -> valid scenario
      if (found_voter_it->second.first->getBlockHash() == kNullBlockHash && vote->getBlockHash() != kNullBlockHash) {
        return {true, nullptr};
      } else if (found_voter_it->second.first->getBlockHash() != kNullBlockHash &&
                 vote->getBlockHash() == kNullBlockHash) {
        return {true, nullptr};
      }
    } else if (found_voter_it->second.second->getHash() == vote->getHash()) {
      return {true, nullptr};
    }
  }

  std::stringstream err;
  err << "Non unique vote: " << ", new vote hash (voted value): " << vote->getHash().abridged() << " ("
      << vote->getBlockHash().abridged() << ")"
      << ", orig. vote hash (voted value): " << found_voter_it->second.first->getHash().abridged() << " ("
      << found_voter_it->second.first->getBlockHash().abridged() << ")";
  if (found_voter_it->second.second != nullptr) {
    err << ", orig. vote 2 hash (voted value): " << found_voter_it->second.second->getHash().abridged() << " ("
        << found_voter_it->second.second->getBlockHash().abridged() << ")";
  }
  err << ", round: " << vote->getRound() << ", step: " << vote->getStep() << ", voter: " << vote->getVoterAddr();
  LOG(log_er_) << err.str();

  // Return existing vote
  if (found_voter_it->second.second && vote->getHash() != found_voter_it->second.second->getHash()) {
    return {false, found_voter_it->second.second};
  }
  return {false, found_voter_it->second.first};
}

std::vector<std::shared_ptr<PbftVote>> VoteManager::getProposalVotes(PbftPeriod period, PbftRound round) const {
  const auto& step_votes = verified_votes_.getStepVotes(period, round, PbftStates::value_proposal_state);
  if (!step_votes) {
    return {};
  }

  std::vector<std::shared_ptr<PbftVote>> proposal_votes;
  for (const auto& voted_value : step_votes->votes) {
    // Multiple nodes might re-propose the same block from previous round
    for (const auto& vote_pair : voted_value.second.votes) {
      proposal_votes.emplace_back(vote_pair.second);
    }
  }

  return proposal_votes;
}

std::optional<PbftRound> VoteManager::determineNewRound(PbftPeriod current_pbft_period, PbftRound current_pbft_round) {
  const auto& period_map = verified_votes_.getPeriodVotes(current_pbft_period);
  if (!period_map) {
    return {};
  }

  for (auto round_rit = period_map->rbegin(); round_rit != period_map->rend(); ++round_rit) {
    // As we keep also previous round verified votes, we have to filter it out
    if (round_rit->first < current_pbft_round) {
      return {};
    }

    // Get either 2t+1 voted null or specific block
    auto found_two_t_plus_one_voted_block =
        round_rit->second.two_t_plus_one_voted_blocks_.find(TwoTPlusOneVotedBlockType::NextVotedBlock);
    if (found_two_t_plus_one_voted_block == round_rit->second.two_t_plus_one_voted_blocks_.end()) {
      found_two_t_plus_one_voted_block =
          round_rit->second.two_t_plus_one_voted_blocks_.find(TwoTPlusOneVotedBlockType::NextVotedNullBlock);
    }

    if (found_two_t_plus_one_voted_block != round_rit->second.two_t_plus_one_voted_blocks_.end()) {
      LOG(log_nf_) << "New round " << round_rit->first + 1 << " determined for period " << current_pbft_period
                   << ". Found 2t+1 votes for block " << found_two_t_plus_one_voted_block->second.hash << " in round "
                   << round_rit->first << ", step " << found_two_t_plus_one_voted_block->second.step;

      return round_rit->first + 1;
    }
  }

  return {};
}

PbftPeriod VoteManager::getRewardVotesPbftBlockPeriod() {
  std::shared_lock lock(reward_votes_info_mutex_);
  return reward_votes_period_;
}

void VoteManager::resetRewardVotes(PbftPeriod period, PbftRound round, PbftStep step, const blk_hash_t& block_hash,
                                   Batch& batch) {
  // Save 2t+1 cert votes to database, remove old reward votes
  {
    std::scoped_lock lock(reward_votes_info_mutex_);
    reward_votes_block_hash_ = block_hash;
    reward_votes_period_ = period;
    reward_votes_round_ = round;
  }

  const auto& round_votes = verified_votes_.getRoundVotes(period, round);
  if (!round_votes) {
    LOG(log_er_) << "resetRewardVotes missing round " << round << " or period " << period;
    assert(false);
    return;
  }

  auto found_step_it = round_votes->step_votes.find(step);
  if (found_step_it == round_votes->step_votes.end()) {
    LOG(log_er_) << "resetRewardVotes missing step" << step;
    assert(false);
    return;
  }

  auto found_two_t_plus_one_voted_block =
      round_votes->two_t_plus_one_voted_blocks_.find(TwoTPlusOneVotedBlockType::CertVotedBlock);
  if (found_two_t_plus_one_voted_block == round_votes->two_t_plus_one_voted_blocks_.end()) {
    LOG(log_er_) << "resetRewardVotes missing cert voted block";
    assert(false);
    return;
  }
  if (found_two_t_plus_one_voted_block->second.hash != block_hash) {
    LOG(log_er_) << "resetRewardVotes incorrect block " << found_two_t_plus_one_voted_block->second.step << " expected "
                 << block_hash;
    assert(false);
    return;
  }
  auto found_voted_value_it = found_step_it->second.votes.find(block_hash);
  if (found_voted_value_it == found_step_it->second.votes.end()) {
    LOG(log_er_) << "resetRewardVotes missing vote block " << block_hash;
    assert(false);
    return;
  }
  std::vector<std::shared_ptr<PbftVote>> votes;
  votes.reserve(found_voted_value_it->second.votes.size());
  for (const auto& tmp_vote : found_voted_value_it->second.votes) {
    votes.push_back(tmp_vote.second);
  }

  db_->replaceTwoTPlusOneVotesToBatch(TwoTPlusOneVotedBlockType::CertVotedBlock, votes, batch);
  db_->removeExtraRewardVotes(extra_reward_votes_, batch);
  extra_reward_votes_.clear();

  LOG(log_dg_) << "Reward votes info reset to: block_hash: " << block_hash << ", period: " << period
               << ", round: " << round;
}

bool VoteManager::isValidRewardVote(const std::shared_ptr<PbftVote>& vote) const {
  std::shared_lock lock(reward_votes_info_mutex_);
  if (vote->getType() != PbftVoteTypes::cert_vote) {
    LOG(log_tr_) << "Invalid reward vote: type " << static_cast<uint64_t>(vote->getType())
                 << " is different from cert type";
    return false;
  }

  if (vote->getBlockHash() != reward_votes_block_hash_) {
    LOG(log_tr_) << "Invalid reward vote: block hash " << vote->getBlockHash()
                 << " is different from reward_votes_block_hash " << reward_votes_block_hash_;
    return false;
  }

  if (vote->getPeriod() != reward_votes_period_) {
    LOG(log_tr_) << "Invalid reward vote: period " << vote->getPeriod()
                 << " is different from reward_votes_block_period " << reward_votes_period_;
    return false;
  }

  // !!! Important: Different nodes might finalize/push the same block in different rounds and therefore they can
  // include different reward votes when creating new block - accept all cert votes with matching period & block_hash
  // round Dummy round protection - if we pushed the block in round reward_votes_round_, the rest of the network should
  // push it shortly after - 100 rounds buffer
  if (vote->getRound() > reward_votes_round_ + 100) {
    LOG(log_wr_) << "Invalid reward vote: round " << vote->getRound() << " exceeded max round "
                 << reward_votes_round_ + 100;
    return false;
  }

  return true;
}

std::pair<bool, std::vector<std::shared_ptr<PbftVote>>> VoteManager::checkRewardVotes(
    const std::shared_ptr<PbftBlock>& pbft_block, bool copy_votes) {
  if (pbft_block->getPeriod() == 1) [[unlikely]] {
    // First period no reward votes
    return {true, {}};
  }

  auto getRewardVotes = [this](const RoundVerifiedVotes& round_votes, const std::vector<vote_hash_t>& vote_hashes,
                               const blk_hash_t& block_hash,
                               bool copy_votes) -> std::pair<bool, std::vector<std::shared_ptr<PbftVote>>> {
    // Get cert votes
    const auto found_step_votes_it = round_votes.step_votes.find(static_cast<PbftStep>(PbftVoteTypes::cert_vote));
    if (found_step_votes_it == round_votes.step_votes.end()) {
      LOG(log_dg_) << "getRewardVotes: No votes found for certify step "
                   << static_cast<PbftStep>(PbftVoteTypes::cert_vote);
      return {false, {}};
    }

    // Find verified votes for specified block_hash
    const auto found_verified_votes_it = found_step_votes_it->second.votes.find(block_hash);
    if (found_verified_votes_it == found_step_votes_it->second.votes.end()) {
      LOG(log_dg_) << "getRewardVotes: No votes found for block_hash " << block_hash;
      return {false, {}};
    }

    std::vector<std::shared_ptr<PbftVote>> found_reward_votes;
    const auto& potential_reward_votes = found_verified_votes_it->second.votes;
    bool found_all_votes = true;
    for (const auto& vote_hash : vote_hashes) {
      const auto found_vote = potential_reward_votes.find(vote_hash);
      if (found_vote == potential_reward_votes.end()) {
        // Reward vote not found
        LOG(log_tr_) << "getRewardVotes: Vote " << vote_hash << " not found";
        found_all_votes = false;
      }

      if (found_all_votes && copy_votes) {
        found_reward_votes.push_back(found_vote->second);
      }
    }

    if (found_all_votes) {
      return {true, std::move(found_reward_votes)};
    } else {
      return {false, {}};
    }
  };

  blk_hash_t reward_votes_block_hash;
  PbftRound reward_votes_period;
  PbftRound reward_votes_round;
  {
    std::shared_lock reward_votes_info_lock(reward_votes_info_mutex_);
    reward_votes_block_hash = reward_votes_block_hash_;
    reward_votes_period = reward_votes_period_;
    reward_votes_round = reward_votes_round_;
  }

  const auto& round_votes = verified_votes_.getRoundVotes(reward_votes_period, reward_votes_round);
  if (!round_votes) {
    LOG(log_er_) << "checkRewardVotes missing round " << reward_votes_round << " or period " << reward_votes_period;
    assert(false);
    return {false, {}};
  }

  const auto reward_votes_hashes = pbft_block->getRewardVotes();

  // Most of the time we should get the reward votes based on reward_votes_period_ and reward_votes_round_
  auto reward_votes = getRewardVotes(*round_votes, reward_votes_hashes, reward_votes_block_hash, copy_votes);
  if (reward_votes.first) [[likely]] {
    return {true, std::move(reward_votes.second)};
  }

  const auto period_votes = verified_votes_.getPeriodVotes(reward_votes_period);
  if (!period_votes) {
    LOG(log_er_) << "checkRewardVotes missing period " << reward_votes_period;
    assert(false);
    return {false, {}};
  }

  // It could happen though in some edge cases that some nodes pushed the same block in different round than we did
  // and when they included the reward votes in new block, these votes have different round than what saved in
  // reward_votes_round_ -> therefore we have to iterate over all rounds and find the correct round
  for (auto round_it = period_votes->rbegin(); round_it != period_votes->rend(); round_it++) {
    const auto tmp_reward_votes =
        getRewardVotes(round_it->second, reward_votes_hashes, reward_votes_block_hash, copy_votes);
    if (!tmp_reward_votes.first) {
      LOG(log_dg_) << "No (or not enough) reward votes found for block " << pbft_block->getBlockHash()
                   << ", period: " << pbft_block->getPeriod()
                   << ", prev. block hash: " << pbft_block->getPrevBlockHash()
                   << ", reward_votes_period: " << reward_votes_period << ", reward_votes_round_: " << round_it->first
                   << ", reward_votes_block_hash: " << reward_votes_block_hash;
      continue;
    }

    return {true, std::move(tmp_reward_votes.second)};
  }

  LOG(log_er_) << "No (or not enough) reward votes found for block " << pbft_block->getBlockHash()
               << ", period: " << pbft_block->getPeriod() << ", prev. block hash: " << pbft_block->getPrevBlockHash()
               << ", reward_votes_period: " << reward_votes_period << ", reward_votes_round_: " << reward_votes_round
               << ", reward_votes_block_hash: " << reward_votes_block_hash;
  return {false, {}};
}

std::vector<std::shared_ptr<PbftVote>> VoteManager::getRewardVotes() {
  blk_hash_t reward_votes_block_hash;
  PbftRound reward_votes_period;
  PbftRound reward_votes_round;
  {
    std::shared_lock reward_votes_info_lock(reward_votes_info_mutex_);
    reward_votes_block_hash = reward_votes_block_hash_;
    reward_votes_period = reward_votes_period_;
    reward_votes_round = reward_votes_round_;
  }

  auto reward_votes =
      getTwoTPlusOneVotedBlockVotes(reward_votes_period, reward_votes_round, TwoTPlusOneVotedBlockType::CertVotedBlock);

  if (!reward_votes.empty() && reward_votes[0]->getBlockHash() != reward_votes_block_hash) {
    // This should never happen
    LOG(log_er_) << "Proposal reward votes block hash mismatch. reward_votes_block_hash " << reward_votes_block_hash
                 << ", reward_votes[0]->getBlockHash() " << reward_votes[0]->getBlockHash();
    assert(false);
    return {};
  }

  return reward_votes;
}

void VoteManager::saveOwnVerifiedVote(const std::shared_ptr<PbftVote>& vote) {
  own_verified_votes_.push_back(vote);
  db_->saveOwnVerifiedVote(vote);
}

std::vector<std::shared_ptr<PbftVote>> VoteManager::getOwnVerifiedVotes() { return own_verified_votes_; }

void VoteManager::clearOwnVerifiedVotes(Batch& write_batch) {
  db_->clearOwnVerifiedVotes(write_batch, own_verified_votes_);
  own_verified_votes_.clear();
}

uint64_t VoteManager::getPbftSortitionThreshold(uint64_t total_dpos_votes_count, PbftVoteTypes vote_type) const {
  switch (vote_type) {
    case PbftVoteTypes::propose_vote:
      return std::min<uint64_t>(kPbftConfig.number_of_proposers, total_dpos_votes_count);
    case PbftVoteTypes::soft_vote:
    case PbftVoteTypes::cert_vote:
    case PbftVoteTypes::next_vote:
    default:
      return std::min<uint64_t>(kPbftConfig.committee_size, total_dpos_votes_count);
  }
}

std::shared_ptr<PbftVote> VoteManager::generateVoteWithWeight(const taraxa::blk_hash_t& blockhash,
                                                              PbftVoteTypes vote_type, PbftPeriod period,
                                                              PbftRound round, PbftStep step,
                                                              const WalletConfig& wallet) {
  uint64_t voter_dpos_votes_count = 0;
  uint64_t total_dpos_votes_count = 0;
  uint64_t pbft_sortition_threshold = 0;

  try {
    voter_dpos_votes_count = final_chain_->dposEligibleVoteCount(period - 1, wallet.node_addr);
    if (!voter_dpos_votes_count) {
      // No delegation
      return nullptr;
    }

    total_dpos_votes_count = final_chain_->dposEligibleTotalVoteCount(period - 1);
    pbft_sortition_threshold = getPbftSortitionThreshold(total_dpos_votes_count, vote_type);

  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to place vote for period: " << period << ", round: " << round << ", step: " << step
                 << ", voted block hash: " << blockhash.abridged() << ". "
                 << "Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what();

    return nullptr;
  }

  auto vote = generateVote(blockhash, vote_type, period, round, step, wallet);
  vote->calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold);

  if (*vote->getWeight() == 0) {
    // zero weight vote
    return nullptr;
  }

  return vote;
}

std::shared_ptr<PbftVote> VoteManager::generateVote(const blk_hash_t& blockhash, PbftVoteTypes type, PbftPeriod period,
                                                    PbftRound round, PbftStep step, const WalletConfig& wallet) {
  // sortition proof
  VrfPbftSortition vrf_sortition(wallet.vrf_secret, {type, period, round, step});
  return std::make_shared<PbftVote>(wallet.node_secret, std::move(vrf_sortition), blockhash);
}

std::pair<bool, std::string> VoteManager::validateVote(const std::shared_ptr<PbftVote>& vote, bool strict) const {
  std::stringstream err_msg;
  const uint64_t vote_period = vote->getPeriod();

  try {
    const uint64_t voter_dpos_votes_count = final_chain_->dposEligibleVoteCount(vote_period - 1, vote->getVoterAddr());

    // Mark vote as validated only after getting dposEligibleVoteCount and other values from dpos contract. It is
    // possible that we are behind in processing pbft blocks, in which case we wont be able to get values from dpos
    // contract and validation fails due to this, not due to the fact that vote is invalid...
    already_validated_votes_.insert(vote->getHash());

    if (voter_dpos_votes_count == 0) {
      err_msg << "Invalid vote " << vote->getHash() << ": author " << vote->getVoterAddr() << " has zero stake";
      return {false, err_msg.str()};
    }

    const auto pk = key_manager_->getVrfKey(vote_period - 1, vote->getVoterAddr());
    if (!pk) {
      err_msg << "No vrf key mapped for vote author " << vote->getVoterAddr();
      return {false, err_msg.str()};
    }

    if (!vote->verifyVote()) {
      err_msg << "Invalid vote " << vote->getHash() << ": invalid signature";
      return {false, err_msg.str()};
    }

    if (!vote->verifyVrfSortition(*pk, strict)) {
      err_msg << "Invalid vote " << vote->getHash() << ": invalid vrf proof";
      return {false, err_msg.str()};
    }

    const uint64_t total_dpos_votes_count = final_chain_->dposEligibleTotalVoteCount(vote_period - 1);
    const uint64_t pbft_sortition_threshold = getPbftSortitionThreshold(total_dpos_votes_count, vote->getType());
    if (!vote->calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold)) {
      err_msg << "Invalid vote " << vote->getHash() << ": zero weight";
      return {false, err_msg.str()};
    }
  } catch (state_api::ErrFutureBlock& e) {
    err_msg << "Unable to validate vote " << vote->getHash() << " against dpos contract. It's period (" << vote_period
            << ") is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
            << "). Err msg: " << e.what();
    return {false, err_msg.str()};
  } catch (...) {
    err_msg << "Invalid vote " << vote->getHash() << ": unknown error during validation";
    return {false, err_msg.str()};
  }

  return {true, ""};
}

std::optional<uint64_t> VoteManager::getPbftTwoTPlusOne(PbftPeriod pbft_period, PbftVoteTypes vote_type) const {
  // Check cache first
  {
    std::shared_lock lock(current_two_t_plus_one_mutex_);
    const auto cached_two_t_plus_one_it = current_two_t_plus_one_.find(vote_type);
    if (cached_two_t_plus_one_it != current_two_t_plus_one_.end()) {
      const auto [cached_period, cached_two_t_plus_one] = cached_two_t_plus_one_it->second;
      if (pbft_period == cached_period && cached_two_t_plus_one) {
        return cached_two_t_plus_one;
      }
    }
  }

  uint64_t total_dpos_votes_count = 0;
  try {
    total_dpos_votes_count = final_chain_->dposEligibleTotalVoteCount(pbft_period);
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to calculate 2t + 1 for period: " << pbft_period
                 << ". Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what();
    return {};
  }

  const auto two_t_plus_one = getPbftSortitionThreshold(total_dpos_votes_count, vote_type) * 2 / 3 + 1;

  // Cache is only for current pbft chain size
  if (pbft_period == pbft_chain_->getPbftChainSize()) {
    std::scoped_lock lock(current_two_t_plus_one_mutex_);
    current_two_t_plus_one_[vote_type] = std::make_pair(pbft_period, two_t_plus_one);
  }

  return two_t_plus_one;
}

bool VoteManager::voteAlreadyValidated(const vote_hash_t& vote_hash) const {
  return already_validated_votes_.contains(vote_hash);
}

bool VoteManager::genAndValidateVrfSortition(PbftPeriod pbft_period, PbftRound pbft_round,
                                             const WalletConfig& wallet) const {
  VrfPbftSortition vrf_sortition(wallet.vrf_secret, {PbftVoteTypes::propose_vote, pbft_period, pbft_round, 1});

  try {
    const uint64_t voter_dpos_votes_count = final_chain_->dposEligibleVoteCount(pbft_period - 1, wallet.node_addr);
    if (!voter_dpos_votes_count) {
      LOG(log_er_) << "Generated vrf sortition for period " << pbft_period << ", round " << pbft_round
                   << " is invalid. Voter dpos vote count is zero";
      return false;
    }

    const uint64_t total_dpos_votes_count = final_chain_->dposEligibleTotalVoteCount(pbft_period - 1);
    const uint64_t pbft_sortition_threshold =
        getPbftSortitionThreshold(total_dpos_votes_count, PbftVoteTypes::propose_vote);

    if (!vrf_sortition.calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold,
                                       wallet.node_pk)) {
      LOG(log_dg_) << "Generated vrf sortition for period " << pbft_period << ", round " << pbft_round
                   << " is invalid. Vrf sortition is zero";
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to generate vrf sortition for period " << pbft_period << ", round " << pbft_round
                 << ". Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what();
    return false;
  }

  return true;
}

std::optional<blk_hash_t> VoteManager::getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                                TwoTPlusOneVotedBlockType type) const {
  const auto& voted = verified_votes_.getTwoTPlusOneVotedBlock(period, round, type);
  if (!voted) {
    return {};
  }
  return voted->hash;
}

std::vector<std::shared_ptr<PbftVote>> VoteManager::getTwoTPlusOneVotedBlockVotes(
    PbftPeriod period, PbftRound round, TwoTPlusOneVotedBlockType type) const {
  return verified_votes_.getTwoTPlusOneVotedBlockVotes(period, round, type);
}

VerifiedVotes::StepVotes VoteManager::getStepVotes(PbftPeriod period, PbftRound round, PbftStep step) const {
  std::shared_lock lock(verified_votes_access_);
  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto found_step_it = found_round_it->second.step_votes.find(step);
  if (found_step_it == found_round_it->second.step_votes.end()) {
    return {};
  }

  return found_step_it->second;
}

}  // namespace taraxa
