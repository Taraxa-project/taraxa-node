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

constexpr PbftStep kExtendedPartionSteps = 1000;
constexpr PbftStep kFirstFinishStep = 4;

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
    for (const auto& vote : votes) {
      // Check if votes are unique per round, step & voter
      if (!isUniqueVote(vote).first) {
        continue;
      }

      addVerifiedVote(vote);
      LOG(log_dg_) << "Vote " << vote->getHash() << " loaded from db to memory";
    }
  };

  loadVotesFromDb(db_->getAllTwoTPlusOneVotes());
  loadVotesFromDb(db_->getOwnVerifiedVotes());
  loadVotesFromDb(db_->getRewardVotes());
}

void VoteManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

void VoteManager::retreieveVotes_() {
  LOG(log_si_) << "Retrieve verified votes from DB";
  auto verified_votes = db_->getOwnVerifiedVotes();
  const auto pbft_step = db_->getPbftMgrField(PbftMgrField::Step);
  for (auto const& v : verified_votes) {
    // Rebroadcast our own next votes in case we were partitioned...
    if (v->getStep() >= kFirstFinishStep && pbft_step > kExtendedPartionSteps) {
      if (auto net = network_.lock()) {
        net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVote(v, nullptr);
      }
    }

    // Check if votes are unique per round & step & voter
    if (auto is_unique_vote = isUniqueVote(v); !is_unique_vote.first) {
      // This should never happen
      assert(false);
    }

    addVerifiedVote(v);
    LOG(log_dg_) << "Retrieved verified vote " << *v;
  }
}

std::vector<std::shared_ptr<Vote>> VoteManager::getVerifiedVotes() const {
  std::vector<std::shared_ptr<Vote>> votes;
  votes.reserve(getVerifiedVotesSize());

  SharedLock lock(verified_votes_access_);
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

  SharedLock lock(verified_votes_access_);
  for (auto const& period : verified_votes_) {
    for (auto const& round : period.second) {
      for (auto const& step : round.second.step_votes) {
        for (auto const& voted_value : step.second.votes) {
          size += voted_value.second.second.size();
        }
      }
    }
  }

  return size;
}

void VoteManager::setCurrentPbftPeriodAndRound(PbftPeriod pbft_period, PbftRound pbft_round) {
  current_pbft_period_ = pbft_period;
  current_pbft_round_ = pbft_round;

  SharedLock lock(verified_votes_access_);

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

bool VoteManager::addVerifiedVote(const std::shared_ptr<Vote>& vote) {
  assert(vote->getWeight().has_value());
  const auto hash = vote->getHash();
  const auto weight = *vote->getWeight();
  if (!weight) {
    LOG(log_er_) << "Unable to add vote " << hash << " into the verified queue. Invalid vote weight";
    return false;
  }

  // TODO: this is locking the same verified_votes_access_ as few lines below
  if (!insertUniqueVote(vote)) {
    LOG(log_wr_) << "Non unique vote " << vote->getHash().abridged() << " (race condition)";
    return false;
  }

  // TODO: solve two_t_plus_one that is received always for cert vote type...
  const auto two_t_plus_one = getPbftTwoTPlusOne(vote->getPeriod() - 1);
  const auto vote_block_hash = vote->getBlockHash();

  {
    UniqueLock lock(verified_votes_access_);

    // It is possible that period just changed and validated vote is now a round behind and possibly a reward vote
    // TODO: if we keep reward votes inside verified_votes_, we would not need this extra handling
    if (vote->getPeriod() < current_pbft_period_) {
      // Old vote, ignore unless it is a reward vote
      if (vote->getPeriod() == current_pbft_period_ - 1 && vote->getType() == PbftVoteTypes::cert_vote) {
        LOG(log_dg_) << "Add vote " << vote->getHash().abridged() << " into the reward votes instead of verified votes";
        addRewardVote(vote, false);
        return false;
      }

      // Old vote, ignore it
      LOG(log_tr_) << "Old vote " << vote->getHash().abridged() << " vote period" << vote->getPeriod()
                   << " current period " << current_pbft_period_;
      return false;
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

    const auto total_weight = (found_voted_value_it->second.first += weight);

    // Set 2t+1 voted block
    if (total_weight >= two_t_plus_one) {
      auto saveTwoTPlusOneVotesInDb = [this, &found_round_it, &found_voted_value_it](TwoTPlusOneVotedBlockType two_plus_one_voted_block_type, const std::shared_ptr<Vote> vote) {
        auto found_two_t_plus_one_voted_block = found_round_it->second.two_t_plus_one_voted_blocks_.find(two_plus_one_voted_block_type);

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
        if (vote->getPeriod() == current_pbft_period_ && vote->getRound() == current_pbft_round_) {
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
  }

  LOG(log_nf_) << "Added verified vote: " << hash;
  LOG(log_dg_) << "Added verified vote: " << *vote;

  return true;
}

bool VoteManager::voteInVerifiedMap(const std::shared_ptr<Vote>& vote) const {
  SharedLock lock(verified_votes_access_);

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
  SharedLock lock(verified_votes_access_);

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

bool VoteManager::insertUniqueVote(const std::shared_ptr<Vote>& vote, bool lock_verified_votes_mutex) {
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
          << ", new vote hash (voted value): " << vote->getHash().abridged() << " (" << vote->getBlockHash().abridged()
          << ")"
          << ", orig. vote hash (voted value): " << inserted_vote.first->second.first->getHash().abridged() << " ("
          << inserted_vote.first->second.first->getBlockHash().abridged() << ")";
      if (inserted_vote.first->second.second != nullptr) {
        err << ", orig. vote 2 hash (voted value): " << inserted_vote.first->second.second->getHash().abridged() << " ("
            << inserted_vote.first->second.second->getBlockHash().abridged() << ")";
      }
      err << ", period: " << vote->getPeriod() << ", round: " << vote->getRound() << ", step: " << vote->getStep()
          << ", voter: " << vote->getVoterAddr();
      LOG(log_er_) << err.str();

      return false;
    }

    return true;
  };

  // TODO: this if else locking will be deleted once we get reward votes from verified_votes_
  if (lock_verified_votes_mutex) {
    UniqueLock lock(verified_votes_access_);
    return insertVote(vote);
  }

  return insertVote(vote);
}

// cleanup votes < pbft_round
void VoteManager::cleanupVotesByRound(PbftPeriod pbft_period, PbftRound pbft_round) {
  // Remove verified votes
  auto batch = db_->createWriteBatch();
  {
    UniqueLock lock(verified_votes_access_);
    auto found_period_it = verified_votes_.find(pbft_period);

    // cleanupVotesByRound should not be called in case there are not votes for previous round
    if (found_period_it == verified_votes_.end()) {
      assert(false);
      return;
    }

    auto round_it = found_period_it->second.begin();
    while (round_it != found_period_it->second.end() && round_it->first < pbft_round) {
      for (const auto& step : round_it->second.step_votes) {
        for (const auto& voted_value : step.second.votes) {
          for (const auto& v : voted_value.second.second) {
            // TODO: if we get reward votes from verified_votes_, we would not need this extra handling...
            if (v.second->getType() == PbftVoteTypes::cert_vote) {
              // The verified cert vote may be reward vote
              addRewardVote(v.second, false);
            }
          }
        }
      }
      round_it = found_period_it->second.erase(round_it);
    }

  }  // verified_votes_access_
  db_->commitWriteBatch(batch);
}

void VoteManager::cleanupVotesByPeriod(PbftPeriod pbft_period) {
  // Remove verified votes
  auto batch = db_->createWriteBatch();
  {
    UniqueLock lock(verified_votes_access_);
    auto it = verified_votes_.begin();
    while (it != verified_votes_.end() && it->first < pbft_period) {
      for (const auto& round : it->second) {
        for (const auto& step : round.second.step_votes) {
          for (const auto& voted_value : step.second.votes) {
            for (const auto& v : voted_value.second.second) {
              if (v.second->getType() == PbftVoteTypes::cert_vote) {
                // The verified cert vote may be reward vote
                // TODO: if we get reward votes from verified_votes_, we would not need this extra handling...
                addRewardVote(v.second, false);
              }
            }
          }
        }
      }

      it = verified_votes_.erase(it);
    }
  }

  db_->commitWriteBatch(batch);
}

std::vector<std::shared_ptr<Vote>> VoteManager::getProposalVotes(PbftPeriod period, PbftRound round) const {
  SharedLock lock(verified_votes_access_);

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
  SharedLock lock(verified_votes_access_);

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

uint64_t VoteManager::getRewardVotesPbftBlockPeriod() {
  std::shared_lock lock(reward_votes_mutex_);
  return reward_votes_pbft_block_.second;
}

bool VoteManager::addRewardVote(const std::shared_ptr<Vote>& vote, bool lock_verified_votes_mutex) {
  std::unique_lock lock(reward_votes_mutex_);
  if (vote->getType() != PbftVoteTypes::cert_vote) {
    LOG(log_wr_) << "Invalid type: " << static_cast<uint64_t>(vote->getType());
    return false;
  }

  if (vote->getStep() != 3) {
    LOG(log_wr_) << "Invalid step: " << vote->getStep();
    return false;
  }

  const auto [reward_votes_block_hash, reward_votes_block_period] = reward_votes_pbft_block_;
  if (vote->getBlockHash() != reward_votes_block_hash) {
    LOG(log_wr_) << "Invalid block hash " << vote->getBlockHash() << " -> different from reward_votes_block_hash "
                 << reward_votes_block_hash;
    return false;
  }

  if (vote->getPeriod() != reward_votes_block_period) {
    LOG(log_wr_) << "Invalid period " << vote->getPeriod() << " -> different from reward_votes_block_period "
                 << reward_votes_block_period;
    return false;
  }

  if (!insertUniqueVote(vote, lock_verified_votes_mutex)) {
    LOG(log_wr_) << "Non unique vote " << vote->getHash().abridged() << " (race condition)";
    return false;
  }

  // If reward vote is from another round it should not be added to last block cert votes which should all be the same
  // round
  if (reward_votes_round_ == vote->getRound()) {
    db_->saveLastBlockCertVote(vote);
  }

  return reward_votes_.insert({vote->getHash(), vote}).second;
}

bool VoteManager::isInRewardsVotes(const vote_hash_t& vote_hash) const {
  std::shared_lock lock(reward_votes_mutex_);
  return reward_votes_.contains(vote_hash);
}

bool VoteManager::checkRewardVotes(const std::shared_ptr<PbftBlock>& pbft_block) {
  if (pbft_block->getPeriod() == 1) [[unlikely]] {
    // First period no reward votes
    return true;
  }

  const auto pbft_block_period = pbft_block->getPeriod();
  const auto& reward_votes_hashes = pbft_block->getRewardVotes();
  std::shared_lock lock(reward_votes_mutex_);
  for (const auto& v : reward_votes_hashes) {
    const auto found_reward_vote = reward_votes_.find(v);
    if (found_reward_vote == reward_votes_.end()) {
      LOG(log_er_) << "Missing reward vote " << v;
      return false;
    }

    // This should never happen
    if (found_reward_vote->second->getPeriod() + 1 != pbft_block_period) [[unlikely]] {
      LOG(log_er_) << "Reward vote has wrong period " << found_reward_vote->second->getPeriod()
                   << ", pbft block period: " << pbft_block_period;
      assert(true);
      return false;
    }
  }

  return true;
}

std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> VoteManager::replaceRewardVotes(
    const std::vector<std::shared_ptr<Vote>>& cert_votes) {
  if (cert_votes.empty()) return {};

  std::unique_lock lock(reward_votes_mutex_);
  auto reward_votes = std::move(reward_votes_);
  reward_votes_.clear();
  reward_votes_pbft_block_ = {cert_votes[0]->getBlockHash(), cert_votes[0]->getPeriod()};

  // It is possible that incoming reward votes might have another round because it is possible that same block was cert
  // voted in different rounds on different nodes but this is a reference round for any pbft block this node might
  // propose
  reward_votes_round_ = cert_votes[0]->getRound();
  for (auto& v : cert_votes) {
    assert(v->getWeight());
    reward_votes_.insert({v->getHash(), std::move(v)});
  }
  return reward_votes;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getAllRewardVotes() {
  std::vector<std::shared_ptr<Vote>> reward_votes;

  std::shared_lock lock(reward_votes_mutex_);
  for (const auto& v : reward_votes_) {
    reward_votes.push_back(v.second);
  }

  return reward_votes;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getRewardVotesByHashes(const std::vector<vote_hash_t>& vote_hashes) {
  std::vector<std::shared_ptr<Vote>> reward_votes;

  std::shared_lock lock(reward_votes_mutex_);
  for (auto vote_hash : vote_hashes) {
    auto it = reward_votes_.find(vote_hash);
    if (it != reward_votes_.end()) {
      reward_votes.emplace_back(it->second);
    } else {
      LOG(log_dg_) << "Missing reward vote: " << vote_hash;
      return {};
    }
  }

  return reward_votes;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getProposeRewardVotes() {
  std::vector<std::shared_ptr<Vote>> reward_votes;

  std::shared_lock lock(reward_votes_mutex_);
  for (const auto& v : reward_votes_) {
    // Select only reward votes with round == round during which node pushed previous block into the chain
    if (v.second->getRound() == reward_votes_round_) {
      reward_votes.push_back(v.second);
    }
  }

  return reward_votes;
}

void VoteManager::sendRewardVotes(const blk_hash_t& pbft_block_hash, bool rebroadcast) {
  {
    std::shared_lock lock(reward_votes_mutex_);
    if (reward_votes_pbft_block_.first != pbft_block_hash) return;
  }

  auto reward_votes = getAllRewardVotes();
  if (reward_votes.empty()) return;

  if (auto net = network_.lock()) {
    net->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->onNewPbftVotesBundle(std::move(reward_votes),
                                                                                             rebroadcast);
  }
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

std::optional<uint64_t> VoteManager::getPbftTwoTPlusOne(PbftPeriod pbft_period) const {
  // Check cache first
  {
    std::shared_lock lock(current_two_t_plus_one_mutex_);
    if (pbft_period == current_two_t_plus_one_.first && current_two_t_plus_one_.second) {
      return current_two_t_plus_one_.second;
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

  const auto two_t_plus_one = getPbftSortitionThreshold(total_dpos_votes_count, PbftVoteTypes::cert_vote) * 2 / 3 + 1;

  // Cache is only for current pbft chain size
  if (pbft_period == pbft_chain_->getPbftChainSize()) {
    std::unique_lock lock(current_two_t_plus_one_mutex_);
    current_two_t_plus_one_ = std::make_pair(pbft_period, two_t_plus_one);
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
  SharedLock lock(verified_votes_access_);

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

std::vector<std::shared_ptr<Vote>> VoteManager::getTwoTPlusOneVotedBlockVotes(
    PbftPeriod period, PbftRound round, TwoTPlusOneVotedBlockType type,
    const std::shared_ptr<network::tarcap::TaraxaPeer>& peer_filter) const {
  SharedLock lock(verified_votes_access_);

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
    if (peer_filter && peer_filter->isVoteKnown(vote.first)) {
      continue;
    }

    votes.push_back(vote.second);
  }

  return votes;
}

std::vector<std::shared_ptr<Vote>> VoteManager::getAllTwoTPlusOneNextVotes(
    PbftPeriod period, PbftRound round, const std::shared_ptr<network::tarcap::TaraxaPeer>& peer_filter) const {
  auto next_votes =
      getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::NextVotedBlock, peer_filter);

  auto null_block_next_vote =
      getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::NextVotedNullBlock, peer_filter);
  if (!null_block_next_vote.empty()) {
    next_votes.reserve(next_votes.size() + null_block_next_vote.size());
    next_votes.insert(next_votes.end(), std::make_move_iterator(null_block_next_vote.begin()),
                      std::make_move_iterator(null_block_next_vote.end()));
  }

  return next_votes;
}

NextVotesManager::NextVotesManager(addr_t node_addr, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<FinalChain> final_chain)
    : db_(std::move(db)), final_chain_(std::move(final_chain)), enough_votes_for_null_block_hash_(false) {
  LOG_OBJECTS_CREATE("NEXT_VOTES");
}

void NextVotesManager::clearVotes() {
  UniqueLock lock(access_);
  enough_votes_for_null_block_hash_ = false;
  voted_value_.reset();
  next_votes_.clear();
  next_votes_weight_.clear();
  next_votes_set_.clear();

  db_->removePreviousRoundNextVotes();
}

bool NextVotesManager::find(vote_hash_t next_vote_hash) {
  SharedLock lock(access_);
  return next_votes_set_.find(next_vote_hash) != next_votes_set_.end();
}

bool NextVotesManager::enoughNextVotes() const {
  SharedLock lock(access_);
  return enough_votes_for_null_block_hash_ && voted_value_.has_value();
}

bool NextVotesManager::haveEnoughVotesForNullBlockHash() const {
  SharedLock lock(access_);
  return enough_votes_for_null_block_hash_;
}

std::optional<blk_hash_t> NextVotesManager::getVotedValue() const {
  SharedLock lock(access_);
  return voted_value_;
}

std::vector<std::shared_ptr<Vote>> NextVotesManager::getNextVotes() {
  std::vector<std::shared_ptr<Vote>> next_votes_bundle;

  SharedLock lock(access_);
  for (auto const& blk_hash_nv : next_votes_) {
    std::copy(blk_hash_nv.second.begin(), blk_hash_nv.second.end(), std::back_inserter(next_votes_bundle));
  }
  return next_votes_bundle;
}

size_t NextVotesManager::getNextVotesWeight() const {
  SharedLock lock(access_);
  size_t size = 0;
  for (const auto& w : next_votes_weight_) {
    size += w.second;
  }
  return size;
}

// Assumption is that all votes are validated, in next voting phase, in the same round.
// Votes for same voted value are in the same step
// Voted values have maximum 2 PBFT block hashes, kNullBlockHash and a non kNullBlockHash
void NextVotesManager::addNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1) {
  if (next_votes.empty()) {
    return;
  }

  if (enoughNextVotes()) {
    LOG(log_dg_) << "Have enough next votes for previous PBFT round.";
    return;
  }

  auto own_votes = getNextVotes();
  const auto sync_voted_round = next_votes[0]->getRound();
  if (!own_votes.empty()) {
    auto own_previous_round = own_votes[0]->getRound();
    if (own_previous_round != sync_voted_round) {
      LOG(log_dg_) << "Drop it. The previous PBFT round has been at " << own_previous_round
                   << ", syncing next votes voted at round " << sync_voted_round;
      return;
    }
  }
  LOG(log_dg_) << "There are " << next_votes.size() << " new next votes for adding.";

  UniqueLock lock(access_);

  auto next_votes_in_db = db_->getPreviousRoundNextVotes();
  if (!next_votes_in_db.empty()) {
    const auto db_next_votes_round = next_votes_in_db[0]->getRound();
    if (db_next_votes_round != sync_voted_round) {
      LOG(log_dg_) << "Drop next votes. db next votes round " << db_next_votes_round << ", received next votes round "
                   << sync_voted_round;
      return;
    }
  }

  // Add all next votes
  std::unordered_set<blk_hash_t> voted_values;
  for (auto const& v : next_votes) {
    LOG(log_dg_) << "Add next vote: " << *v;

    auto vote_hash = v->getHash();
    if (next_votes_set_.contains(vote_hash)) {
      continue;
    }

    next_votes_set_.insert(vote_hash);
    auto voted_block_hash = v->getBlockHash();
    next_votes_[voted_block_hash].emplace_back(v);
    next_votes_weight_[voted_block_hash] += v->getWeight().value();

    voted_values.insert(voted_block_hash);
    next_votes_in_db.emplace_back(v);
  }

  if (voted_values.empty()) {
    LOG(log_dg_) << "No new unique votes received";
    return;
  }

  // Update list of next votes in database by new unique votes
  db_->savePreviousRoundNextVotes(next_votes_in_db);

  LOG(log_dg_) << "PBFT 2t+1 is " << pbft_2t_plus_1 << " in round " << next_votes[0]->getRound();
  for (auto const& voted_value : voted_values) {
    auto const& voted_value_next_votes_size = next_votes_weight_[voted_value];
    if (voted_value_next_votes_size >= pbft_2t_plus_1) {
      LOG(log_dg_) << "Voted PBFT block hash " << voted_value << " has enough " << voted_value_next_votes_size
                   << " next votes";

      if (voted_value == kNullBlockHash) {
        enough_votes_for_null_block_hash_ = true;
      } else {
        if (voted_value_.has_value() && voted_value != *voted_value_) {
          assertError_(next_votes_.at(voted_value), next_votes_.at(*voted_value_));
        }

        voted_value_ = voted_value;
      }
    } else {
      // Should not happen here, have checked at updateWithSyncedVotes. For safe
      LOG(log_dg_) << "Should not happen. Voted PBFT block hash " << voted_value << " has "
                   << voted_value_next_votes_size << " next votes. Not enough, removed!";
      for (auto const& v : next_votes_.at(voted_value)) {
        next_votes_set_.erase(v->getHash());
      }
      next_votes_.erase(voted_value);
      next_votes_weight_.erase(voted_value);
    }
  }

  if (next_votes_.size() != 1 && next_votes_.size() != 2) {
    LOG(log_er_) << "There are " << next_votes_.size() << " voted values.";
    for (auto const& voted_value : next_votes_) {
      LOG(log_er_) << "Voted value " << voted_value.first;
    }
    assert(next_votes_.size() == 1 || next_votes_.size() == 2);
  }
}

// Assumption is that all votes are validated, in next voting phase, in the same round and step
void NextVotesManager::updateNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1) {
  LOG(log_nf_) << "There are " << next_votes.size() << " next votes for updating.";
  if (next_votes.empty()) {
    return;
  }

  // Deletes current previous round next votes
  clearVotes();

  UniqueLock lock(access_);

  // TODO: refactor this as we know most of the required info in place where updateNextVotes is called
  // Copy all next votes
  for (auto const& v : next_votes) {
    LOG(log_dg_) << "Add next vote: " << *v;
    assert(v->getWeight());

    next_votes_set_.insert(v->getHash());
    auto voted_block_hash = v->getBlockHash();
    if (next_votes_.contains(voted_block_hash)) {
      next_votes_[voted_block_hash].emplace_back(v);
      next_votes_weight_[voted_block_hash] += v->getWeight().value();
    } else {
      next_votes_[voted_block_hash] = {v};
      next_votes_weight_[voted_block_hash] = v->getWeight().value();
    }
  }

  // Protect for malicious players. If no malicious players, will include either/both NULL BLOCK HASH and a non NULL
  // BLOCK HASH
  LOG(log_nf_) << "PBFT 2t+1 is " << pbft_2t_plus_1 << " in round " << next_votes[0]->getRound();
  auto it = next_votes_.begin();
  while (it != next_votes_.end()) {
    if (next_votes_weight_[it->first] >= pbft_2t_plus_1) {
      LOG(log_nf_) << "Voted PBFT block hash " << it->first << " has " << next_votes_weight_[it->first]
                   << " next votes";

      if (it->first == kNullBlockHash) {
        enough_votes_for_null_block_hash_ = true;
      } else {
        if (voted_value_.has_value()) {
          assertError_(it->second, next_votes_.at(*voted_value_));
        }

        voted_value_ = it->first;
      }

      it++;
    } else {
      LOG(log_dg_) << "Voted PBFT block hash " << it->first << " has " << next_votes_weight_[it->first]
                   << " next votes. Not enough, removed!";
      for (auto const& v : it->second) {
        next_votes_set_.erase(v->getHash());
      }
      next_votes_weight_.erase(it->first);
      it = next_votes_.erase(it);
    }
  }

  if (next_votes_.size() != 1 && next_votes_.size() != 2) {
    LOG(log_er_) << "There are " << next_votes_.size() << " voted values.";
    for (auto const& voted_value : next_votes_) {
      LOG(log_er_) << "Voted value " << voted_value.first;
    }
    assert(next_votes_.size() == 1 || next_votes_.size() == 2);
  }

  // Save new selected next votes into db
  std::vector<std::shared_ptr<Vote>> db_next_votes;
  for (const auto& voted_block_votes : next_votes_) {
    for (const auto& vote : voted_block_votes.second) {
      db_next_votes.emplace_back(vote);
    }
  }
  db_->savePreviousRoundNextVotes(db_next_votes);
}

// Assumption is that all synced votes are in next voting phase, in the same round.
// Valid voted values have maximum 2 block hash, kNullBlockHash and a non kNullBlockHash
void NextVotesManager::updateWithSyncedVotes(std::vector<std::shared_ptr<Vote>>& next_votes, size_t pbft_2t_plus_1) {
  if (next_votes.empty()) {
    LOG(log_er_) << "Synced next votes is empty.";
    return;
  }

  if (enoughNextVotes()) {
    LOG(log_dg_) << "Don't need update. Have enough next votes for previous PBFT round already.";
    return;
  }

  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> own_votes_map;
  {
    SharedLock lock(access_);
    own_votes_map = next_votes_;
  }

  if (own_votes_map.empty()) {
    LOG(log_nf_) << "Own next votes for previous round is empty. Node just start, reject for protecting overwrite own "
                    "next votes.";
    return;
  }

  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> synced_next_votes;
  std::unordered_map<blk_hash_t, uint64_t> synced_next_votes_weight;
  for (auto& vote : next_votes) {
    auto voted_block_hash = vote->getBlockHash();
    if (synced_next_votes.count(voted_block_hash)) {
      synced_next_votes[voted_block_hash].emplace_back(vote);
      synced_next_votes_weight[voted_block_hash] += vote->getWeight().value();
    } else {
      std::vector<std::shared_ptr<Vote>> votes{vote};
      synced_next_votes[voted_block_hash] = std::move(votes);
      synced_next_votes_weight[voted_block_hash] = vote->getWeight().value();
    }
  }

  // Next votes for same voted value should be in the same step
  for (auto const& voted_value_and_votes : synced_next_votes) {
    const auto& votes = voted_value_and_votes.second;
    const auto voted_step = votes[0]->getStep();
    const auto voted_period = votes[0]->getPeriod();
    for (size_t i = 1; i < votes.size(); i++) {
      if (votes[i]->getStep() != voted_step) {
        LOG(log_er_) << "Synced next votes have a different voted PBFT step. Vote1 " << *votes[0] << ", Vote2 "
                     << *votes[i];
        return;
      }
      if (votes[i]->getPeriod() != voted_period) {
        LOG(log_er_) << "Synced next votes have a different voted PBFT period. Vote1 " << *votes[0] << ", Vote2 "
                     << *votes[i];
        return;
      }
    }
  }

  if (synced_next_votes.size() != 1 && synced_next_votes.size() != 2) {
    LOG(log_er_) << "Synced next contain more than 2 voted values " << synced_next_votes.size();
    for (auto const& voted_value_and_votes : synced_next_votes) {
      LOG(log_er_) << "Synced next votes voted value " << voted_value_and_votes.first;
    }
    return;
  }

  std::vector<std::shared_ptr<Vote>> update_votes;
  // Don't update votes for same valid voted value that >= 2t+1
  for (auto const& voted_value_and_votes : synced_next_votes) {
    if (own_votes_map.count(voted_value_and_votes.first)) {
      continue;
    }

    if (synced_next_votes_weight[voted_value_and_votes.first] >= pbft_2t_plus_1) {
      LOG(log_nf_) << "Don't have the voted value " << voted_value_and_votes.first << " for previous round. Add votes";
      for (auto const& v : voted_value_and_votes.second) {
        LOG(log_dg_) << "Add next vote " << *v;
        update_votes.emplace_back(v);
      }
    } else {
      LOG(log_dg_) << "Voted value " << voted_value_and_votes.first
                   << " doesn't have enough next votes. Size of syncing next votes "
                   << synced_next_votes_weight[voted_value_and_votes.first] << ", PBFT previous round 2t+1 is "
                   << pbft_2t_plus_1 << " for round " << voted_value_and_votes.second[0]->getRound();
    }
  }

  // Adds new next votes in internal structures + DB for the PBFT round
  addNextVotes(update_votes, pbft_2t_plus_1);
}

void NextVotesManager::assertError_(std::vector<std::shared_ptr<Vote>> next_votes_1,
                                    std::vector<std::shared_ptr<Vote>> next_votes_2) const {
  if (next_votes_1.empty() || next_votes_2.empty()) {
    return;
  }

  LOG(log_er_) << "There are more than one voted values on non kNullBlockHash have 2t+1 next votes.";

  LOG(log_er_) << "Voted value " << next_votes_1[0]->getBlockHash();
  for (auto const& v : next_votes_1) {
    LOG(log_er_) << *v;
  }
  LOG(log_er_) << "Voted value " << next_votes_2[0]->getBlockHash();
  for (auto const& v : next_votes_2) {
    LOG(log_er_) << *v;
  }

  assert(false);
}

}  // namespace taraxa
