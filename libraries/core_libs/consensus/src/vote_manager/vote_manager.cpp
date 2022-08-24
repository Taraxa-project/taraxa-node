#include "vote_manager/vote_manager.hpp"

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <optional>
#include <shared_mutex>

#include "network/network.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"

constexpr size_t EXTENDED_PARTITION_STEPS = 1000;
constexpr size_t FIRST_FINISH_STEP = 4;

namespace taraxa {
VoteManager::VoteManager(const addr_t& node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<FinalChain> final_chain, std::shared_ptr<NextVotesManager> next_votes_mgr)
    : db_(std::move(db)),
      pbft_chain_(std::move(pbft_chain)),
      final_chain_(std::move(final_chain)),
      next_votes_manager_(std::move(next_votes_mgr)) {
  LOG_OBJECTS_CREATE("VOTE_MGR");

  // Retrieve votes from DB
  daemon_ = std::make_unique<std::thread>([this]() { retreieveVotes_(); });
}

VoteManager::~VoteManager() { daemon_->join(); }

void VoteManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

void VoteManager::retreieveVotes_() {
  LOG(log_si_) << "Retrieve verified votes from DB";
  auto verified_votes = db_->getVerifiedVotes();
  const auto pbft_step = db_->getPbftMgrField(PbftMgrRoundStep::PbftStep);
  for (auto const& v : verified_votes) {
    // Rebroadcast our own next votes in case we were partitioned...
    if (v->getStep() >= FIRST_FINISH_STEP && pbft_step > EXTENDED_PARTITION_STEPS) {
      std::vector<std::shared_ptr<Vote>> votes = {v};
      if (auto net = network_.lock()) {
        net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes(std::move(votes));
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
  for (const auto& round : verified_votes_) {
    for (const auto& period : round.second) {
      for (const auto& step : period.second) {
        for (const auto& voted_value : step.second) {
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
  for (auto const& round : verified_votes_) {
    for (auto const& period : round.second) {
      for (auto const& step : period.second) {
        for (auto const& voted_value : step.second) {
          size += voted_value.second.second.size();
        }
      }
    }
  }

  return size;
}

bool VoteManager::addVerifiedVote(std::shared_ptr<Vote> const& vote) {
  assert(vote->getWeight().has_value());
  const auto hash = vote->getHash();
  const auto weight = *vote->getWeight();
  if (!weight) {
    LOG(log_er_) << "Unable to add vote " << hash << " into the verified queue. Invalid vote weight";
    return false;
  }

  if (!insertUniqueVote(vote)) {
    LOG(log_wr_) << "Non unique vote " << vote->getHash().abridged() << " (race condition)";
    return false;
  }

  UniqueLock lock(verified_votes_access_);

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

  auto found_step_it = found_round_it->second.find(vote->getStep());
  // Add step
  if (found_step_it == found_round_it->second.end()) {
    found_step_it = found_round_it->second.insert({vote->getStep(), {}}).first;
  }

  auto found_voted_value_it = found_step_it->second.find(vote->getBlockHash());
  // Add voted value
  if (found_voted_value_it == found_step_it->second.end()) {
    found_voted_value_it = found_step_it->second.insert({vote->getBlockHash(), {}}).first;
  }

  if (found_voted_value_it->second.second.contains(hash)) {
    LOG(log_dg_) << "Vote " << hash << " is in verified map already";
    return false;
  }

  // Add vote hash
  if (!found_voted_value_it->second.second.insert({hash, vote}).second) {
    // This should never happen
    assert(false);
  }

  found_voted_value_it->second.first += weight;

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

  const auto found_step_it = found_round_it->second.find(vote->getStep());
  if (found_step_it == found_round_it->second.end()) {
    return false;
  }

  const auto found_voted_value_it = found_step_it->second.find(vote->getBlockHash());
  if (found_voted_value_it == found_step_it->second.end()) {
    return false;
  }

  return found_voted_value_it->second.second.find(vote->getHash()) != found_voted_value_it->second.second.end();
}

std::pair<bool, std::string> VoteManager::isUniqueVote(const std::shared_ptr<Vote>& vote) const {
  std::shared_lock lock(voters_unique_votes_mutex_);

  const auto found_period_it = voters_unique_votes_.find(vote->getPeriod());
  if (found_period_it == voters_unique_votes_.end()) {
    return {true, ""};
  }

  const auto found_round_it = found_period_it->second.find(vote->getRound());
  if (found_round_it == found_period_it->second.end()) {
    return {true, ""};
  }

  const auto found_step_it = found_round_it->second.find(vote->getStep());
  if (found_step_it == found_round_it->second.end()) {
    return {true, ""};
  }

  const auto found_voter_it = found_step_it->second.find(vote->getVoterAddr());
  if (found_voter_it == found_step_it->second.end()) {
    return {true, ""};
  }

  if (found_voter_it->second.first->getHash() == vote->getHash()) {
    return {true, ""};
  }

  // Next votes are special case, where we allow voting for both NULL_BLOCK_HASH and some other specific block hash
  // at the same time
  if (vote->getType() == PbftVoteTypes::next_vote_type) {
    // New second next vote
    if (found_voter_it->second.second == nullptr) {
      // One of the next votes == NULL_BLOCK_HASH -> valid scenario
      if (found_voter_it->second.first->getBlockHash() == NULL_BLOCK_HASH && vote->getBlockHash() != NULL_BLOCK_HASH) {
        return {true, ""};
      } else if (found_voter_it->second.first->getBlockHash() != NULL_BLOCK_HASH &&
                 vote->getBlockHash() == NULL_BLOCK_HASH) {
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
  std::unique_lock lock(voters_unique_votes_mutex_);

  auto found_period_it = voters_unique_votes_.find(vote->getPeriod());
  if (found_period_it == voters_unique_votes_.end()) {
    found_period_it = voters_unique_votes_.insert({vote->getPeriod(), {}}).first;
  }

  auto found_round_it = found_period_it->second.find(vote->getRound());
  if (found_round_it == found_period_it->second.end()) {
    found_round_it = found_period_it->second.insert({vote->getRound(), {}}).first;
  }

  auto found_step_it = found_round_it->second.find(vote->getStep());
  if (found_step_it == found_round_it->second.end()) {
    found_step_it = found_round_it->second.insert({vote->getStep(), {}}).first;
  }

  auto inserted_vote = found_step_it->second.insert({vote->getVoterAddr(), {vote, nullptr}});

  // Vote was successfully inserted -> it is unique
  if (inserted_vote.second) {
    return true;
  }

  // There was already some vote inserted, check if it is the same vote as we are trying to insert
  if (inserted_vote.first->second.first->getHash() != vote->getHash()) {
    // Next votes (second finishing steps) are special case, where we allow voting for both NULL_BLOCK_HASH and
    // some other specific block hash at the same time -> 2 unique votes per round & step & voter
    if (vote->getType() == PbftVoteTypes::next_vote_type && vote->getStep() % 2) {
      // New second next vote
      if (inserted_vote.first->second.second == nullptr) {
        // One of the next votes == NULL_BLOCK_HASH -> valid scenario
        if (inserted_vote.first->second.first->getBlockHash() == NULL_BLOCK_HASH &&
            vote->getBlockHash() != NULL_BLOCK_HASH) {
          inserted_vote.first->second.second = vote;
          return true;
        } else if (inserted_vote.first->second.first->getBlockHash() != NULL_BLOCK_HASH &&
                   vote->getBlockHash() == NULL_BLOCK_HASH) {
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
}

// cleanup votes < pbft_round
void VoteManager::cleanupVotesByRound(uint64_t pbft_period, uint64_t pbft_round) {
  // Clean up cache with unique votes per period, round & step & voter
  {
    std::unique_lock unique_votes_lock(voters_unique_votes_mutex_);

    auto found_period_it = voters_unique_votes_.find(pbft_period);
    if (found_period_it != voters_unique_votes_.end()) {
      auto it = found_period_it->second.begin();
      while (it != found_period_it->second.end() && it->first < pbft_round) {
        it = found_period_it->second.erase(it);
      }
    }
  }

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
      for (const auto& step : round_it->second) {
        for (const auto& voted_value : step.second) {
          for (const auto& v : voted_value.second.second) {
            if (v.second->getType() == cert_vote_type) {
              // The verified cert vote may be reward vote
              addRewardVote(v.second);
            }
            db_->removeVerifiedVoteToBatch(v.first, batch);
            LOG(log_dg_) << "Remove verified vote " << v.first << " for period, round = " << v.second->getPeriod()
                         << ", " << v.second->getRound() << ". PBFT round " << pbft_round;
          }
        }
      }
      round_it = found_period_it->second.erase(round_it);
    }

  }  // verified_votes_access_
  db_->commitWriteBatch(batch);
}

void VoteManager::cleanupVotesByPeriod(uint64_t pbft_period) {
  // Clean up cache with unique votes per period, round & step & voter
  {
    std::unique_lock unique_votes_lock(voters_unique_votes_mutex_);

    // Prior periods...
    auto it = voters_unique_votes_.begin();
    while (it != voters_unique_votes_.end() && it->first < pbft_period) {
      it = voters_unique_votes_.erase(it);
    }
  }

  // Remove verified votes
  auto batch = db_->createWriteBatch();
  {
    UniqueLock lock(verified_votes_access_);
    auto it = verified_votes_.begin();
    while (it != verified_votes_.end() && it->first < pbft_period) {
      for (const auto& round : it->second) {
        for (const auto& step : round.second) {
          for (const auto& voted_value : step.second) {
            for (const auto& v : voted_value.second.second) {
              if (v.second->getType() == cert_vote_type) {
                // The verified cert vote may be reward vote
                // TODO: would be nice to get rid of this...
                addRewardVote(v.second);
              }

              db_->removeVerifiedVoteToBatch(v.first, batch);
              LOG(log_dg_) << "Remove verified vote " << v.first << " vote period " << v.second->getPeriod()
                           << ", vote round " << v.second->getRound() << ". new PBFT period " << pbft_period;
            }
          }
        }
      }

      it = verified_votes_.erase(it);
    }
  }

  db_->commitWriteBatch(batch);
}

// TODO: Refactor call to put period before round
std::vector<std::shared_ptr<Vote>> VoteManager::getProposalVotes(uint64_t round, uint64_t period) const {
  SharedLock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto found_proposal_step_it = found_round_it->second.find(1);
  if (found_proposal_step_it == found_round_it->second.end()) {
    return {};
  }

  std::vector<std::shared_ptr<Vote>> proposal_votes;
  for (const auto& voted_value : found_proposal_step_it->second) {
    // Multiple nodes might re-propose the same block from previous round
    for (const auto& vote_pair : voted_value.second.second) {
      proposal_votes.emplace_back(vote_pair.second);
    }
  }

  return proposal_votes;
}

// TODO: Refactor call to put period before round
std::optional<VotesBundle> VoteManager::getVotesBundle(uint64_t round, uint64_t period, size_t step,
                                                       size_t two_t_plus_one) const {
  SharedLock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto found_step_it = found_round_it->second.find(step);
  if (found_step_it == found_round_it->second.end()) {
    return {};
  }

  VotesBundle votes_bundle;
  for (const auto& voted_value : found_step_it->second) {
    if (voted_value.second.first < two_t_plus_one) {
      continue;
    }

    // It should never happen that there is 2t+1 votes for the same round and possibly different period or different
    // voted value
    assert(votes_bundle.votes.empty());

    size_t count = 0;
    votes_bundle.votes.reserve(voted_value.second.second.size());
    for (const auto& v : voted_value.second.second) {
      assert(v.second->getWeight().has_value());
      votes_bundle.votes.emplace_back(v.second);
      count += *v.second->getWeight();

      // For certify votes - collect all votes, for all other vote types, collect just 2t+1 votes
      if (step != certify_state && count >= two_t_plus_one) {
        break;
      }
    }

    votes_bundle.voted_block_hash = voted_value.first;
    votes_bundle.votes_period = period;
    LOG(log_nf_) << "Found enough " << count << " votes at voted value " << votes_bundle.voted_block_hash
                 << ", period: " << period << ", round: " << round << ", step " << step;

    // TODO: use "return votes_bundle;" here ?
    // We could return here "return votes_bundle;", continue here is just to check if there is not any other period or
    // voted value with 2t+1 votes, which would mean consensus is broken
    continue;
  }

  // Not enough votes were found
  if (votes_bundle.votes.empty()) {
    return {};
  }

  return votes_bundle;
}

uint64_t VoteManager::determineRoundFromPeriodAndVotes(uint64_t period, size_t two_t_plus_one) {
  std::vector<std::shared_ptr<Vote>> votes;

  SharedLock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return 1;
  }

  for (auto round_rit = found_period_it->second.rbegin(); round_rit != found_period_it->second.rend(); ++round_rit) {
    for (auto step_rit = round_rit->second.rbegin(); step_rit != round_rit->second.rend(); ++step_rit) {
      if (step_rit->first <= 3) {
        break;
      }

      for (auto const& voted_value : step_rit->second) {
        if (voted_value.second.first < two_t_plus_one) {
          continue;
        }

        auto it = voted_value.second.second.begin();
        size_t count = 0;

        // Copy at least 2t+1 votes
        while (count < two_t_plus_one) {
          votes.emplace_back(it->second);
          count += it->second->getWeight().value();
          it++;
        }
        LOG(log_nf_) << "Round determined for period " << period << ". Found " << count << " votes for voted value "
                     << voted_value.first << " for round " << round_rit->first << ", step " << step_rit->first;

        next_votes_manager_->updateNextVotes(votes, two_t_plus_one);
        return round_rit->first + 1;
      }
    }
  }

  return 1;
}

std::pair<blk_hash_t, uint64_t> VoteManager::getCurrentRewardsVotesBlock() const {
  std::shared_lock lock(reward_votes_mutex_);
  return reward_votes_pbft_block_;
}

bool VoteManager::addRewardVote(const std::shared_ptr<Vote>& vote) {
  std::unique_lock lock(reward_votes_mutex_);
  if (vote->getType() != cert_vote_type) {
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

  if (!insertUniqueVote(vote)) {
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

void VoteManager::replaceRewardVotes(std::vector<std::shared_ptr<Vote>>&& cert_votes) {
  if (cert_votes.empty()) return;

  std::unique_lock lock(reward_votes_mutex_);
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

void VoteManager::sendRewardVotes(const blk_hash_t& pbft_block_hash) {
  {
    std::shared_lock lock(reward_votes_mutex_);
    if (reward_votes_pbft_block_.first != pbft_block_hash) return;
  }

  auto reward_votes = getAllRewardVotes();
  if (reward_votes.empty()) return;

  auto net = network_.lock();
  assert(net);
  net->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVotes(std::move(reward_votes));
}

NextVotesManager::NextVotesManager(addr_t node_addr, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<FinalChain> final_chain)
    : db_(std::move(db)),
      final_chain_(std::move(final_chain)),
      enough_votes_for_null_block_hash_(false),
      voted_value_(NULL_BLOCK_HASH),
      voted_period_(0) {
  LOG_OBJECTS_CREATE("NEXT_VOTES");
}

void NextVotesManager::clear() {
  UniqueLock lock(access_);
  enough_votes_for_null_block_hash_ = false;
  voted_value_ = NULL_BLOCK_HASH;
  voted_period_ = 0;
  next_votes_.clear();
  next_votes_weight_.clear();
  next_votes_set_.clear();
}

bool NextVotesManager::find(vote_hash_t next_vote_hash) {
  SharedLock lock(access_);
  return next_votes_set_.find(next_vote_hash) != next_votes_set_.end();
}

bool NextVotesManager::enoughNextVotes() const {
  SharedLock lock(access_);
  return enough_votes_for_null_block_hash_ && (voted_value_ != NULL_BLOCK_HASH);
}

bool NextVotesManager::haveEnoughVotesForNullBlockHash() const {
  SharedLock lock(access_);
  return enough_votes_for_null_block_hash_;
}

std::pair<blk_hash_t, uint64_t> NextVotesManager::getVotedValue() const {
  SharedLock lock(access_);
  return {voted_value_, voted_period_};
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
// Voted values have maximum 2 PBFT block hashes, NULL_BLOCK_HASH and a non NULL_BLOCK_HASH
void NextVotesManager::addNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1) {
  if (next_votes.empty()) {
    return;
  }

  if (enoughNextVotes()) {
    LOG(log_dg_) << "Have enough next votes for prevous PBFT round.";
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

  auto next_votes_in_db = db_->getNextVotes(sync_voted_round);

  // Add all next votes
  std::unordered_set<blk_hash_t> voted_values;
  for (auto const& v : next_votes) {
    LOG(log_dg_) << "Add next vote: " << *v;

    auto vote_hash = v->getHash();
    if (next_votes_set_.count(vote_hash)) {
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
  db_->saveNextVotes(sync_voted_round, next_votes_in_db);

  LOG(log_dg_) << "PBFT 2t+1 is " << pbft_2t_plus_1 << " in round " << next_votes[0]->getRound();
  for (auto const& voted_value : voted_values) {
    auto const& voted_value_next_votes_size = next_votes_weight_[voted_value];
    if (voted_value_next_votes_size >= pbft_2t_plus_1) {
      LOG(log_dg_) << "Voted PBFT block hash " << voted_value << " has enough " << voted_value_next_votes_size
                   << " next votes";

      if (voted_value == NULL_BLOCK_HASH) {
        enough_votes_for_null_block_hash_ = true;
      } else {
        if (voted_value_ != NULL_BLOCK_HASH && voted_value != voted_value_) {
          assertError_(next_votes_.at(voted_value), next_votes_.at(voted_value_));
        }

        voted_value_ = voted_value;
        voted_period_ = next_votes[0]->getPeriod();
      }
    } else {
      // Should not happen here, have checked at updateWithSyncedVotes. For safe
      LOG(log_dg_) << "Shoud not happen here. Voted PBFT block hash " << voted_value << " has "
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

  clear();

  UniqueLock lock(access_);

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

      if (it->first == NULL_BLOCK_HASH) {
        enough_votes_for_null_block_hash_ = true;
      } else {
        if (voted_value_ != NULL_BLOCK_HASH) {
          assertError_(it->second, next_votes_.at(voted_value_));
        }

        voted_value_ = it->first;
        voted_period_ = it->second[0]->getPeriod();
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
}

// Assumption is that all synced votes are in next voting phase, in the same round.
// Valid voted values have maximum 2 block hash, NULL_BLOCK_HASH and a non NULL_BLOCK_HASH
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

  size_t previous_round_sortition_threshold =
      db_->getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold);
  uint64_t previous_round_dpos_period =
      db_->getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod);
  size_t previous_round_dpos_total_votes_count =
      db_->getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount);
  LOG(log_nf_) << "Previous round sortition threshold " << previous_round_sortition_threshold
               << ", previous round DPOS period " << previous_round_dpos_period
               << ", previous round DPOS total votes count " << previous_round_dpos_total_votes_count;

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
    LOG(log_er_) << "Synced next votes voted " << synced_next_votes.size() << " values";
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

  LOG(log_er_) << "There are more than one voted values on non NULL_BLOCK_HASH have 2t+1 next votes.";

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
