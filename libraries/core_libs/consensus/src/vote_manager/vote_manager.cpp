#include "vote_manager/vote_manager.hpp"

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <optional>
#include <shared_mutex>

#include "network/network.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa {

VoteManager::VoteManager(const addr_t& node_addr, const PbftConfig& pbft_config, const secret_t& node_sk,
                         const vrf_wrapper::vrf_sk_t& vrf_sk, std::shared_ptr<DbStorage> db,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<FinalChain> final_chain,
                         std::shared_ptr<KeyManager> key_manager)
    : kNodeAddr(node_addr),
      kPbftConfig(pbft_config),
      kVrfSk(vrf_sk),
      kNodeSk(node_sk),
      kNodePub(dev::toPublic(kNodeSk)),
      db_(std::move(db)),
      pbft_chain_(std::move(pbft_chain)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      already_validated_votes_(1000000, 1000) {
  LOG_OBJECTS_CREATE("VOTE_MGR");

  auto db_votes = db_->getAllTwoTPlusOneVotes();

  auto loadVotesFromDb = [this](const std::vector<std::shared_ptr<Vote>>& votes) {
    bool reward_votes_info_set = false;
    for (const auto& vote : votes) {
      // Check if votes are unique per round, step & voter
      if (!isUniqueVote(vote).first) {
        continue;
      }

      if (!reward_votes_info_set && vote->getType() == PbftVoteTypes::cert_vote) {
        reward_votes_info_set = true;
        reward_votes_block_hash_ = vote->getBlockHash();
        reward_votes_period_ = vote->getPeriod();
        reward_votes_round_ = vote->getRound();
      }

      addVerifiedVote(vote);
      LOG(log_dg_) << "Vote " << vote->getHash() << " loaded from db to memory";
    }
  };

  loadVotesFromDb(db_->getAllTwoTPlusOneVotes());
  loadVotesFromDb(db_->getOwnVerifiedVotes());
  auto reward_votes = db_->getRewardVotes();
  for (const auto& vote : reward_votes) extra_reward_votes_.emplace_back(vote->getHash());
  loadVotesFromDb(reward_votes);
}

void VoteManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

std::vector<std::shared_ptr<Vote>> VoteManager::getVerifiedVotes() const {
  std::vector<std::shared_ptr<Vote>> votes;
  votes.reserve(getVerifiedVotesSize());

  std::shared_lock lock(verified_votes_access_);
  for (const auto& period : verified_votes_) {
    for (const auto& round : period.second) {
      for (const auto& step : round.second.step_votes) {
        for (const auto& voted_value : step.second.votes) {
          for (const auto& v : voted_value.second.second) {
            votes.emplace_back(v.second);
          }
        }
      }
    }
  }

  return votes;
}

uint64_t VoteManager::getVerifiedVotesSize() const {
  uint64_t size = 0;

  std::shared_lock lock(verified_votes_access_);
  for (auto const& period : verified_votes_) {
    for (auto const& round : period.second) {
      for (auto const& step : round.second.step_votes) {
        size += std::accumulate(
            step.second.votes.begin(), step.second.votes.end(), 0,
            [](uint64_t value, const auto& voted_value) { return value + voted_value.second.second.size(); });
      }
    }
  }

  return size;
}

void VoteManager::setCurrentPbftPeriodAndRound(PbftPeriod pbft_period, PbftRound pbft_round) {
  current_pbft_period_ = pbft_period;
  current_pbft_round_ = pbft_round;

  std::shared_lock lock(verified_votes_access_);

  auto found_period_it = verified_votes_.find(pbft_period);
  if (found_period_it == verified_votes_.end()) {
    return;
  }

  auto found_round_it = found_period_it->second.find(pbft_round);
  if (found_round_it == found_period_it->second.end()) {
    return;
  }

  // Check if we already have 2t+1 votes bundles for specified pbft period & round. If so, save those votes into db
  // During normal node operation this should happen rarely - it can happen only if we receive 2t+1 future votes for
  // a period or round that we are not yet in
  for (const auto& two_t_plus_one_voted_block : found_round_it->second.two_t_plus_one_voted_blocks_) {
    const TwoTPlusOneVotedBlockType two_t_plus_one_voted_block_type = two_t_plus_one_voted_block.first;
    const auto& [two_t_plus_one_voted_block_hash, two_t_plus_one_voted_block_step] = two_t_plus_one_voted_block.second;

    const auto found_step_votes_it = found_round_it->second.step_votes.find(two_t_plus_one_voted_block_step);
    if (found_step_votes_it == found_round_it->second.step_votes.end()) {
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

    std::vector<std::shared_ptr<Vote>> votes;
    votes.reserve(found_verified_votes_it->second.second.size());
    for (const auto& vote : found_verified_votes_it->second.second) {
      votes.push_back(vote.second);
    }

    db_->replaceTwoTPlusOneVotes(two_t_plus_one_voted_block_type, votes);
  }
}

PbftStep VoteManager::getNetworkTplusOneNextVotingStep(PbftPeriod period, PbftRound round) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return 0;
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return 0;
  }

  return found_round_it->second.network_t_plus_one_step;
}

bool VoteManager::addVerifiedVote(const std::shared_ptr<Vote>& vote) {
  assert(vote->getWeight().has_value());
  const auto hash = vote->getHash();
  const auto weight = *vote->getWeight();
  if (!weight) {
    LOG(log_er_) << "Unable to add vote " << hash << " into the verified queue. Invalid vote weight";
    return false;
  }

  const auto vote_block_hash = vote->getBlockHash();

  {
    std::scoped_lock lock(verified_votes_access_);

    if (!insertUniqueVote(vote)) {
      LOG(log_wr_) << "Non unique vote " << vote->getHash().abridged() << " (race condition)";
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

    auto found_period_it = verified_votes_.find(vote->getPeriod());
    // Add period
    if (found_period_it == verified_votes_.end()) {
      found_period_it = verified_votes_.insert({vote->getPeriod(), {}}).first;
    }

    auto found_round_it = found_period_it->second.find(vote->getRound());
    // Add round
    if (found_round_it == found_period_it->second.end()) {
      found_round_it = found_period_it->second.insert({vote->getRound(), {}}).first;
    }

    auto found_step_it = found_round_it->second.step_votes.find(vote->getStep());
    // Add step
    if (found_step_it == found_round_it->second.step_votes.end()) {
      found_step_it = found_round_it->second.step_votes.insert({vote->getStep(), {}}).first;
    }

    auto found_voted_value_it = found_step_it->second.votes.find(vote_block_hash);
    // Add voted value
    if (found_voted_value_it == found_step_it->second.votes.end()) {
      found_voted_value_it = found_step_it->second.votes.insert({vote_block_hash, {}}).first;
    }

    if (found_voted_value_it->second.second.contains(hash)) {
      LOG(log_dg_) << "Vote " << hash << " is in verified map already";
      return false;
    }

    // Add vote hash
    if (!found_voted_value_it->second.second.insert({hash, vote}).second) {
      // This should never happen
      LOG(log_dg_) << "Vote " << hash << " is in verified map already (race condition)";
      assert(false);
      return false;
    }

    LOG(log_nf_) << "Added verified vote: " << hash;
    LOG(log_dg_) << "Added verified vote: " << *vote;

    if (is_valid_potential_reward_vote) {
      extra_reward_votes_.emplace_back(vote->getHash());
      db_->saveExtraRewardVote(vote);
    }

    const auto total_weight = (found_voted_value_it->second.first += weight);

    // Unable to get 2t+1
    const auto two_t_plus_one = getPbftTwoTPlusOne(vote->getPeriod() - 1, vote->getType());
    if (!two_t_plus_one.has_value()) [[unlikely]] {
      LOG(log_er_) << "Cannot set(or not) 2t+1 voted block as 2t+1 threshold is unavailable, vote " << vote->getHash();
      return true;
    }

    // Calculate t+1
    const auto t_plus_one = ((*two_t_plus_one - 1) / 2) + 1;
    // Set network_t_plus_one_step - used for triggering exponential backoff
    if (vote->getType() == PbftVoteTypes::next_vote && total_weight >= t_plus_one &&
        vote->getStep() > found_round_it->second.network_t_plus_one_step) {
      found_round_it->second.network_t_plus_one_step = vote->getStep();
      LOG(log_nf_) << "Set t+1 next voted block " << vote->getHash() << " in step " << vote->getStep();
    }

    // Not enough votes - do not set 2t+1 voted block for period,round and step
    if (total_weight < *two_t_plus_one) {
      return true;
    }

    // Function to save 2t+1 voted block + its votes
    auto saveTwoTPlusOneVotesInDb = [this, &found_round_it, &found_voted_value_it](
                                        TwoTPlusOneVotedBlockType two_plus_one_voted_block_type,
                                        const std::shared_ptr<Vote> vote) {
      auto found_two_t_plus_one_voted_block =
          found_round_it->second.two_t_plus_one_voted_blocks_.find(two_plus_one_voted_block_type);

      // 2t+1 votes block already set
      if (found_two_t_plus_one_voted_block != found_round_it->second.two_t_plus_one_voted_blocks_.end()) {
        assert(found_two_t_plus_one_voted_block->second.first == vote->getBlockHash());

        // It is possible to have 2t+1 next votes for the same block in multiple steps
        if (two_plus_one_voted_block_type != TwoTPlusOneVotedBlockType::NextVotedBlock &&
            two_plus_one_voted_block_type != TwoTPlusOneVotedBlockType::NextVotedNullBlock) {
          assert(found_two_t_plus_one_voted_block->second.second == vote->getStep());
        }

        return;
      }

      // Insert new 2t+1 voted block
      found_round_it->second.two_t_plus_one_voted_blocks_.insert(
          {two_plus_one_voted_block_type, std::make_pair(vote->getBlockHash(), vote->getStep())});

      // Save only current pbft period & round 2t+1 votes bundles into db
      // Cert votes are saved once the pbft block is pushed in the chain
      if (vote->getType() != PbftVoteTypes::cert_vote && vote->getPeriod() == current_pbft_period_ &&
          vote->getRound() == current_pbft_round_) {
        std::vector<std::shared_ptr<Vote>> votes;
        votes.reserve(found_voted_value_it->second.second.size());
        for (const auto& tmp_vote : found_voted_value_it->second.second) {
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

bool VoteManager::voteInVerifiedMap(const std::shared_ptr<Vote>& vote) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(vote->getPeriod());
  if (found_period_it == verified_votes_.end()) {
    return false;
  }

  const auto found_round_it = found_period_it->second.find(vote->getRound());
  if (found_round_it == found_period_it->second.end()) {
    return false;
  }

  const auto found_step_it = found_round_it->second.step_votes.find(vote->getStep());
  if (found_step_it == found_round_it->second.step_votes.end()) {
    return false;
  }

  const auto found_voted_value_it = found_step_it->second.votes.find(vote->getBlockHash());
  if (found_voted_value_it == found_step_it->second.votes.end()) {
    return false;
  }

  return found_voted_value_it->second.second.find(vote->getHash()) != found_voted_value_it->second.second.end();
}

std::pair<bool, std::string> VoteManager::isUniqueVote(const std::shared_ptr<Vote>& vote) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(vote->getPeriod());
  if (found_period_it == verified_votes_.end()) {
    return {true, ""};
  }

  const auto found_round_it = found_period_it->second.find(vote->getRound());
  if (found_round_it == found_period_it->second.end()) {
    return {true, ""};
  }

  const auto found_step_it = found_round_it->second.step_votes.find(vote->getStep());
  if (found_step_it == found_round_it->second.step_votes.end()) {
    return {true, ""};
  }

  const auto found_voter_it = found_step_it->second.unique_voters.find(vote->getVoterAddr());
  if (found_voter_it == found_step_it->second.unique_voters.end()) {
    return {true, ""};
  }

  if (found_voter_it->second.first->getHash() == vote->getHash()) {
    return {true, ""};
  }

  // Next votes are special case, where we allow voting for both kNullBlockHash and some other specific block hash
  // at the same time
  if (vote->getType() == PbftVoteTypes::next_vote) {
    // New second next vote
    if (found_voter_it->second.second == nullptr) {
      // One of the next votes == kNullBlockHash -> valid scenario
      if (found_voter_it->second.first->getBlockHash() == kNullBlockHash && vote->getBlockHash() != kNullBlockHash) {
        return {true, ""};
      } else if (found_voter_it->second.first->getBlockHash() != kNullBlockHash &&
                 vote->getBlockHash() == kNullBlockHash) {
        return {true, ""};
      }
    } else if (found_voter_it->second.second->getHash() == vote->getHash()) {
      return {true, ""};
    }
  }

  std::stringstream err;
  err << "Non unique vote: "
      << ", new vote hash (voted value): " << vote->getHash().abridged() << " (" << vote->getBlockHash().abridged()
      << ")"
      << ", orig. vote hash (voted value): " << found_voter_it->second.first->getHash().abridged() << " ("
      << found_voter_it->second.first->getBlockHash().abridged() << ")";
  if (found_voter_it->second.second != nullptr) {
    err << ", orig. vote 2 hash (voted value): " << found_voter_it->second.second->getHash().abridged() << " ("
        << found_voter_it->second.second->getBlockHash().abridged() << ")";
  }
  err << ", round: " << vote->getRound() << ", step: " << vote->getStep() << ", voter: " << vote->getVoterAddr();
  return {false, err.str()};
}

bool VoteManager::insertUniqueVote(const std::shared_ptr<Vote>& vote) {
  auto insertVote = [this](const std::shared_ptr<Vote>& vote) -> bool {
    auto found_period_it = verified_votes_.find(vote->getPeriod());
    if (found_period_it == verified_votes_.end()) {
      found_period_it = verified_votes_.insert({vote->getPeriod(), {}}).first;
    }

    auto found_round_it = found_period_it->second.find(vote->getRound());
    if (found_round_it == found_period_it->second.end()) {
      found_round_it = found_period_it->second.insert({vote->getRound(), {}}).first;
    }

    auto found_step_it = found_round_it->second.step_votes.find(vote->getStep());
    if (found_step_it == found_round_it->second.step_votes.end()) {
      found_step_it = found_round_it->second.step_votes.insert({vote->getStep(), {}}).first;
    }

    auto inserted_vote = found_step_it->second.unique_voters.insert({vote->getVoterAddr(), {vote, nullptr}});

    // Vote was successfully inserted -> it is unique
    if (inserted_vote.second) {
      return true;
    }

    // There was already some vote inserted, check if it is the same vote as we are trying to insert
    if (inserted_vote.first->second.first->getHash() != vote->getHash()) {
      // Next votes (second finishing steps) are special case, where we allow voting for both kNullBlockHash and
      // some other specific block hash at the same time -> 2 unique votes per round & step & voter
      if (vote->getType() == PbftVoteTypes::next_vote && vote->getStep() % 2) {
        // New second next vote
        if (inserted_vote.first->second.second == nullptr) {
          // One of the next votes == kNullBlockHash -> valid scenario
          if (inserted_vote.first->second.first->getBlockHash() == kNullBlockHash &&
              vote->getBlockHash() != kNullBlockHash) {
            inserted_vote.first->second.second = vote;
            return true;
          } else if (inserted_vote.first->second.first->getBlockHash() != kNullBlockHash &&
                     vote->getBlockHash() == kNullBlockHash) {
            inserted_vote.first->second.second = vote;
            return true;
          }
        } else if (inserted_vote.first->second.second->getHash() == vote->getHash()) {
          return true;
        }
      }

      std::stringstream err;
      err << "Unable to insert new unique vote(race condition): "
          << ", new vote hash (voted value): " << vote->getHash().abridged() << " (" << vote->getBlockHash() << ")"
          << ", orig. vote hash (voted value): " << inserted_vote.first->second.first->getHash().abridged() << " ("
          << inserted_vote.first->second.first->getBlockHash() << ")";
      if (inserted_vote.first->second.second != nullptr) {
        err << ", orig. vote 2 hash (voted value): " << inserted_vote.first->second.second->getHash().abridged() << " ("
            << inserted_vote.first->second.second->getBlockHash() << ")";
      }
      err << ", period: " << vote->getPeriod() << ", round: " << vote->getRound() << ", step: " << vote->getStep()
          << ", voter: " << vote->getVoterAddr();
      LOG(log_er_) << err.str();

      return false;
    }

    return true;
  };

  return insertVote(vote);
}

void VoteManager::cleanupVotesByPeriod(PbftPeriod pbft_period) {
  // Remove verified votes
  std::scoped_lock lock(verified_votes_access_);
  auto it = verified_votes_.begin();
  while (it != verified_votes_.end() && it->first < pbft_period) {
    it = verified_votes_.erase(it);
  }
}

std::vector<std::shared_ptr<Vote>> VoteManager::getProposalVotes(PbftPeriod period, PbftRound round) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto found_proposal_step_it = found_round_it->second.step_votes.find(PbftStates::value_proposal_state);
  if (found_proposal_step_it == found_round_it->second.step_votes.end()) {
    return {};
  }

  std::vector<std::shared_ptr<Vote>> proposal_votes;
  for (const auto& voted_value : found_proposal_step_it->second.votes) {
    // Multiple nodes might re-propose the same block from previous round
    for (const auto& vote_pair : voted_value.second.second) {
      proposal_votes.emplace_back(vote_pair.second);
    }
  }

  return proposal_votes;
}

std::optional<PbftRound> VoteManager::determineNewRound(PbftPeriod current_pbft_period, PbftRound current_pbft_round) {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(current_pbft_period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  for (auto round_rit = found_period_it->second.rbegin(); round_rit != found_period_it->second.rend(); ++round_rit) {
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
                   << ". Found 2t+1 votes for block " << found_two_t_plus_one_voted_block->second.first << " in round "
                   << round_rit->first << ", step " << found_two_t_plus_one_voted_block->second.second;

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
                                   DbStorage::Batch& batch) {
  // Save 2t+1 cert votes to database, remove old reward votes
  {
    std::scoped_lock lock(reward_votes_info_mutex_);
    reward_votes_block_hash_ = block_hash;
    reward_votes_period_ = period;
    reward_votes_round_ = round;
  }

  std::scoped_lock lock(verified_votes_access_);
  auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    LOG(log_er_) << "resetRewardVotes missing period";
    assert(false);
    return;
  }
  auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    LOG(log_er_) << "resetRewardVotes missing round" << round;
    assert(false);
    return;
  }
  auto found_step_it = found_round_it->second.step_votes.find(step);
  if (found_step_it == found_round_it->second.step_votes.end()) {
    LOG(log_er_) << "resetRewardVotes missing step" << step;
    assert(false);
    return;
  }
  auto found_two_t_plus_one_voted_block =
      found_round_it->second.two_t_plus_one_voted_blocks_.find(TwoTPlusOneVotedBlockType::CertVotedBlock);
  if (found_two_t_plus_one_voted_block == found_round_it->second.two_t_plus_one_voted_blocks_.end()) {
    LOG(log_er_) << "resetRewardVotes missing cert voted block";
    assert(false);
    return;
  }
  if (found_two_t_plus_one_voted_block->second.first != block_hash) {
    LOG(log_er_) << "resetRewardVotes incorrect block " << found_two_t_plus_one_voted_block->second.first
                 << " expected " << block_hash;
    assert(false);
    return;
  }
  auto found_voted_value_it = found_step_it->second.votes.find(block_hash);
  if (found_voted_value_it == found_step_it->second.votes.end()) {
    LOG(log_er_) << "resetRewardVotes missing vote block " << block_hash;
    assert(false);
    return;
  }
  std::vector<std::shared_ptr<Vote>> votes;
  votes.reserve(found_voted_value_it->second.second.size());
  for (const auto& tmp_vote : found_voted_value_it->second.second) {
    votes.push_back(tmp_vote.second);
  }

  db_->replaceTwoTPlusOneVotesToBatch(TwoTPlusOneVotedBlockType::CertVotedBlock, votes, batch);
  db_->removeExtraRewardVotes(extra_reward_votes_, batch);
  extra_reward_votes_.clear();

  LOG(log_dg_) << "Reward votes info reset to: block_hash: " << block_hash << ", period: " << period
               << ", round: " << round;
}

bool VoteManager::isValidRewardVote(const std::shared_ptr<Vote>& vote) const {
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

std::pair<bool, std::vector<std::shared_ptr<Vote>>> VoteManager::checkRewardVotes(
    const std::shared_ptr<PbftBlock>& pbft_block, bool copy_votes) {
  if (pbft_block->getPeriod() == 1) [[unlikely]] {
    // First period no reward votes
    return {true, {}};
  }

  auto getRewardVotes = [this](const std::map<PbftRound, VerifiedVotes>::iterator round_votes_it,
                               const std::vector<vote_hash_t>& vote_hashes, const blk_hash_t& block_hash,
                               bool copy_votes) -> std::pair<bool, std::vector<std::shared_ptr<Vote>>> {
    // Get cert votes
    const auto found_step_votes_it =
        round_votes_it->second.step_votes.find(static_cast<PbftStep>(PbftVoteTypes::cert_vote));
    if (found_step_votes_it == round_votes_it->second.step_votes.end()) {
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

    std::vector<std::shared_ptr<Vote>> found_reward_votes;
    const auto& potential_reward_votes = found_verified_votes_it->second.second;
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
  std::shared_lock verified_votes_lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(reward_votes_period);
  if (found_period_it == verified_votes_.end()) {
    LOG(log_er_) << "No reward votes found for period " << reward_votes_period;
    assert(false);
    return {false, {}};
  }

  const auto found_round_it = found_period_it->second.find(reward_votes_round);
  if (found_round_it == found_period_it->second.end()) {
    LOG(log_er_) << "No reward votes found for round " << reward_votes_round;
    assert(false);
    return {false, {}};
  }

  const auto reward_votes_hashes = pbft_block->getRewardVotes();

  // Most of the time we should get the reward votes based on reward_votes_period_ and reward_votes_round_
  auto reward_votes = getRewardVotes(found_round_it, reward_votes_hashes, reward_votes_block_hash, copy_votes);
  if (reward_votes.first) [[likely]] {
    return {true, std::move(reward_votes.second)};
  }

  // It could happen though in some edge cases that some nodes pushed the same block in different round than we did
  // and when they included the reward votes in new block, these votes have different round than what saved in
  // reward_votes_round_ -> therefore we have to iterate over all rounds and find the correct round
  for (auto round_it = found_period_it->second.begin(); round_it != found_period_it->second.end(); round_it++) {
    const auto tmp_reward_votes = getRewardVotes(round_it, reward_votes_hashes, reward_votes_block_hash, copy_votes);
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

std::vector<std::shared_ptr<Vote>> VoteManager::getRewardVotes() {
  blk_hash_t reward_votes_block_hash;
  PbftRound reward_votes_period;
  PbftRound reward_votes_round;
  {
    std::shared_lock reward_votes_info_lock(reward_votes_info_mutex_);
    reward_votes_block_hash = reward_votes_block_hash_;
    reward_votes_period = reward_votes_period_;
    reward_votes_round = reward_votes_round_;
  }
  std::shared_lock lock(verified_votes_access_);
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

std::shared_ptr<Vote> VoteManager::generateVoteWithWeight(const taraxa::blk_hash_t& blockhash, PbftVoteTypes vote_type,
                                                          PbftPeriod period, PbftRound round, PbftStep step) {
  uint64_t voter_dpos_votes_count = 0;
  uint64_t total_dpos_votes_count = 0;
  uint64_t pbft_sortition_threshold = 0;

  try {
    voter_dpos_votes_count = final_chain_->dpos_eligible_vote_count(period - 1, kNodeAddr);
    if (!voter_dpos_votes_count) {
      // No delegation
      return nullptr;
    }

    total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(period - 1);
    pbft_sortition_threshold = getPbftSortitionThreshold(total_dpos_votes_count, vote_type);

  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to place vote for period: " << period << ", round: " << round << ", step: " << step
                 << ", voted block hash: " << blockhash.abridged() << ". "
                 << "Period is too far ahead of actual finalized pbft chain size (" << final_chain_->last_block_number()
                 << "). Err msg: " << e.what();

    return nullptr;
  }

  auto vote = generateVote(blockhash, vote_type, period, round, step);
  vote->calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold);

  if (*vote->getWeight() == 0) {
    // zero weight vote
    return nullptr;
  }

  return vote;
}

std::shared_ptr<Vote> VoteManager::generateVote(const blk_hash_t& blockhash, PbftVoteTypes type, PbftPeriod period,
                                                PbftRound round, PbftStep step) {
  // sortition proof
  VrfPbftSortition vrf_sortition(kVrfSk, {type, period, round, step});
  return std::make_shared<Vote>(kNodeSk, std::move(vrf_sortition), blockhash);
}

std::pair<bool, std::string> VoteManager::validateVote(const std::shared_ptr<Vote>& vote) const {
  std::stringstream err_msg;
  const uint64_t vote_period = vote->getPeriod();

  try {
    const uint64_t voter_dpos_votes_count =
        final_chain_->dpos_eligible_vote_count(vote_period - 1, vote->getVoterAddr());
    const uint64_t total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(vote_period - 1);

    // Mark vote as validated only after getting dpos_eligible_vote_count and other values from dpos contract. It is
    // possible that we are behind in processing pbft blocks, in which case we wont be able to get values from dpos
    // contract and validation fails due to this, not due to the fact that vote is invalid...
    already_validated_votes_.insert(vote->getHash());

    if (voter_dpos_votes_count == 0) {
      err_msg << "Invalid vote " << vote->getHash() << ": author " << vote->getVoterAddr() << " has zero stake";
      return {false, err_msg.str()};
    }

    const auto pk = key_manager_->get(vote_period - 1, vote->getVoterAddr());
    if (!pk) {
      err_msg << "No vrf key mapped for vote author " << vote->getVoterAddr();
      return {false, err_msg.str()};
    }

    if (!vote->verifyVote()) {
      err_msg << "Invalid vote " << vote->getHash() << ": invalid signature";
      return {false, err_msg.str()};
    }

    if (!vote->verifyVrfSortition(*pk)) {
      err_msg << "Invalid vote " << vote->getHash() << ": invalid vrf proof";
      return {false, err_msg.str()};
    }

    const uint64_t pbft_sortition_threshold = getPbftSortitionThreshold(total_dpos_votes_count, vote->getType());
    if (!vote->calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold)) {
      err_msg << "Invalid vote " << vote->getHash() << ": zero weight";
      return {false, err_msg.str()};
    }
  } catch (state_api::ErrFutureBlock& e) {
    err_msg << "Unable to validate vote " << vote->getHash() << " against dpos contract. It's period (" << vote_period
            << ") is too far ahead of actual finalized pbft chain size (" << final_chain_->last_block_number()
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
    total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(pbft_period);
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to calculate 2t + 1 for period: " << pbft_period
                 << ". Period is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what();
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

bool VoteManager::genAndValidateVrfSortition(PbftPeriod pbft_period, PbftRound pbft_round) const {
  VrfPbftSortition vrf_sortition(kVrfSk, {PbftVoteTypes::propose_vote, pbft_period, pbft_round, 1});

  try {
    const uint64_t voter_dpos_votes_count = final_chain_->dpos_eligible_vote_count(pbft_period - 1, kNodeAddr);
    if (!voter_dpos_votes_count) {
      LOG(log_er_) << "Generated vrf sortition for period " << pbft_period << ", round " << pbft_round
                   << " is invalid. Voter dpos vote count is zero";
      return false;
    }

    const uint64_t total_dpos_votes_count = final_chain_->dpos_eligible_total_vote_count(pbft_period - 1);
    const uint64_t pbft_sortition_threshold =
        getPbftSortitionThreshold(total_dpos_votes_count, PbftVoteTypes::propose_vote);

    if (!vrf_sortition.calculateWeight(voter_dpos_votes_count, total_dpos_votes_count, pbft_sortition_threshold,
                                       kNodePub)) {
      LOG(log_dg_) << "Generated vrf sortition for period " << pbft_period << ", round " << pbft_round
                   << " is invalid. Vrf sortition is zero";
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Unable to generate vrf sorititon for period " << pbft_period << ", round " << pbft_round
                 << ". Period is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what();
    return false;
  }

  return true;
}

std::optional<blk_hash_t> VoteManager::getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                                TwoTPlusOneVotedBlockType type) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto two_t_plus_one_voted_block_it = found_round_it->second.two_t_plus_one_voted_blocks_.find(type);
  if (two_t_plus_one_voted_block_it == found_round_it->second.two_t_plus_one_voted_blocks_.end()) {
    return {};
  }

  return two_t_plus_one_voted_block_it->second.first;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getTwoTPlusOneVotedBlockVotes(PbftPeriod period, PbftRound round,
                                                                              TwoTPlusOneVotedBlockType type) const {
  std::shared_lock lock(verified_votes_access_);
  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto two_t_plus_one_voted_block_it = found_round_it->second.two_t_plus_one_voted_blocks_.find(type);
  if (two_t_plus_one_voted_block_it == found_round_it->second.two_t_plus_one_voted_blocks_.end()) {
    return {};
  }
  const auto [two_t_plus_one_voted_block_hash, two_t_plus_one_voted_block_step] = two_t_plus_one_voted_block_it->second;

  // Find step votes for specified step based on found 2t+1 voted block of type "type"
  const auto found_step_votes_it = found_round_it->second.step_votes.find(two_t_plus_one_voted_block_step);
  if (found_step_votes_it == found_round_it->second.step_votes.end()) {
    assert(false);
    return {};
  }

  // Find verified votes for specified block_hash based on found 2t+1 voted block of type "type"
  const auto found_verified_votes_it = found_step_votes_it->second.votes.find(two_t_plus_one_voted_block_hash);
  if (found_verified_votes_it == found_step_votes_it->second.votes.end()) {
    assert(false);
    return {};
  }

  std::vector<std::shared_ptr<Vote>> votes;
  votes.reserve(found_verified_votes_it->second.second.size());
  for (const auto& vote : found_verified_votes_it->second.second) {
    votes.push_back(vote.second);
  }

  return votes;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getAllTwoTPlusOneNextVotes(PbftPeriod period, PbftRound round) const {
  auto next_votes = getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock);

  auto null_block_next_vote =
      getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock);
  if (!null_block_next_vote.empty()) {
    next_votes.reserve(next_votes.size() + null_block_next_vote.size());
    next_votes.insert(next_votes.end(), std::make_move_iterator(null_block_next_vote.begin()),
                      std::make_move_iterator(null_block_next_vote.end()));
  }

  return next_votes;
}

}  // namespace taraxa
