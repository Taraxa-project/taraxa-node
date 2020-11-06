#include "vote.hpp"

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include "pbft_manager.hpp"
#include "sortition.hpp"

namespace taraxa {
VrfPbftSortition::VrfPbftSortition(bytes const& b) {
  dev::RLP const rlp(b);
  if (!rlp.isList()) throw std::invalid_argument("VrfPbftSortition RLP must be a list");
  pk = rlp[0].toHash<vrf_pk_t>();
  pbft_msg.blk = rlp[1].toHash<blk_hash_t>();
  pbft_msg.type = PbftVoteTypes(rlp[2].toInt<uint>());
  pbft_msg.round = rlp[3].toInt<uint64_t>();
  pbft_msg.step = rlp[4].toInt<size_t>();
  proof = rlp[5].toHash<vrf_proof_t>();
  verify();
}
bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;
  s.appendList(6);
  s << pk;
  s << pbft_msg.blk;
  s << pbft_msg.type;
  s << pbft_msg.round;
  s << pbft_msg.step;
  s << proof;
  return s.out();
}

/*
 * Sortition return true:
 * CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
 * i.e., CREDENTIAL * VALID PLAYERS <= SORTITION THRESHOLD * SIGNATURE_HASH_MAX
 * otherwise return false
 */
bool VrfPbftSortition::canSpeak(size_t threshold, size_t valid_players) const {
  uint1024_t left = (uint1024_t)((uint512_t)output) * valid_players;
  uint1024_t right = (uint1024_t)max512bits * threshold;
  return left <= right;
}

Vote::Vote(dev::RLP const& rlp) {
  if (!rlp.isList()) throw std::invalid_argument("vote RLP must be a list");
  blockhash_ = rlp[0].toHash<blk_hash_t>();
  vrf_sortition_ = VrfPbftSortition(rlp[1].toBytes());
  vote_signatue_ = rlp[2].toHash<sig_t>();
  vote_hash_ = sha3(true);
}

Vote::Vote(bytes const& b) : Vote(dev::RLP(b)) {}

Vote::Vote(secret_t const& node_sk, VrfPbftSortition const& vrf_sortition, blk_hash_t const& blockhash)
    : vrf_sortition_(vrf_sortition), blockhash_(blockhash) {
  vote_signatue_ = dev::sign(node_sk, sha3(false));
  vote_hash_ = sha3(true);
}

bytes Vote::rlp(bool inc_sig) const {
  dev::RLPStream s;
  s.appendList(inc_sig ? 3 : 2);

  s << blockhash_;
  s << vrf_sortition_.getRlpBytes();
  if (inc_sig) {
    s << vote_signatue_;
  }

  return s.out();
}

void Vote::voter() const {
  if (cached_voter_) return;
  cached_voter_ = dev::recover(vote_signatue_, sha3(false));
  assert(cached_voter_);
}

bool VoteManager::voteValidation(taraxa::blk_hash_t const& last_pbft_block_hash, taraxa::Vote const& vote, size_t const valid_sortition_players,
                                 size_t const sortition_threshold) const {
  if (last_pbft_block_hash != vote.getVrfSortition().pbft_msg.blk) {
    LOG(log_tr_) << "Last pbft block hash does not match " << last_pbft_block_hash;
    return false;
  }

  if (!vote.getVrfSortition().verify()) {
    LOG(log_wr_) << "Invalid vrf proof, vote hash " << vote.getHash();
    return false;
  }

  if (!vote.verifyVote()) {
    LOG(log_wr_) << "Invalid vote signature, vote hash " << vote.getHash();
    return false;
  }

  if (!vote.verifyCanSpeak(sortition_threshold, valid_sortition_players)) {
    LOG(log_wr_) << "Vote sortition failed, sortition proof " << vote.getSortitionProof();
    return false;
  }
  return true;
}

bool VoteManager::addVote(taraxa::Vote const& vote) {
  uint64_t pbft_round = vote.getRound();
  auto hash = vote.getHash();

  {
    upgradableLock_ lock(access_);
    std::map<uint64_t, std::map<vote_hash_t, Vote>>::const_iterator found_round = unverified_votes_.find(pbft_round);
    if (found_round != unverified_votes_.end()) {
      std::map<vote_hash_t, Vote>::const_iterator found_vote = found_round->second.find(hash);
      if (found_vote != found_round->second.end()) {
        return false;
      }
      upgradeLock_ locked(lock);
      unverified_votes_[pbft_round][hash] = vote;
    } else {
      std::map<vote_hash_t, Vote> votes{std::make_pair(hash, vote)};
      upgradeLock_ locked(lock);
      unverified_votes_[pbft_round] = votes;
    }
  }
  LOG(log_dg_) << "Add vote " << hash << ", block hash " << vote.getBlockHash() << ", vote type " << vote.getType() << ", in round " << pbft_round
               << ", for step " << vote.getStep();
  return true;
}

// cleanup votes < pbft_round
void VoteManager::cleanupVotes(uint64_t pbft_round) {
  upgradableLock_ lock(access_);
  std::map<uint64_t, std::map<vote_hash_t, Vote>>::iterator it = unverified_votes_.begin();

  upgradeLock_ locked(lock);
  while (it != unverified_votes_.end() && it->first < pbft_round) {
    it = unverified_votes_.erase(it);
  }
}

void VoteManager::clearUnverifiedVotesTable() {
  uniqueLock_ lock(access_);
  unverified_votes_.clear();
}

uint64_t VoteManager::getUnverifiedVotesSize() const {
  uint64_t size = 0;

  sharedLock_ lock(access_);
  std::map<uint64_t, std::map<vote_hash_t, Vote>>::const_iterator it;
  for (it = unverified_votes_.begin(); it != unverified_votes_.end(); ++it) {
    size += it->second.size();
  }

  return size;
}

// Return all verified votes >= pbft_round
// For unit test only
std::vector<Vote> VoteManager::getVotes(uint64_t pbft_round, size_t eligible_voter_count, blk_hash_t last_pbft_block_hash,
                                        size_t sortition_threshold) {
  cleanupVotes(pbft_round);

  std::vector<Vote> verified_votes;

  auto votes_to_verify = getAllVotes();
  for (auto const& v : votes_to_verify) {
    // vote verification
    addr_t vote_address = dev::toAddress(v.getVoter());
    std::pair<val_t, bool> account_balance = final_chain_->getBalance(vote_address);
    if (!account_balance.second) {
      // New node join, it doesn't have other nodes info.
      // Wait unit sync PBFT chain with peers, and execute to get states.
      LOG(log_dg_) << "Cannot find the vote account balance. vote hash: " << v.getHash() << " vote address: " << vote_address;
      continue;
    }
    if (voteValidation(last_pbft_block_hash, v, eligible_voter_count, sortition_threshold)) {
      verified_votes.emplace_back(v);
    }
  }

  return verified_votes;
}

// Return all verified votes >= pbft_round
std::vector<Vote> VoteManager::getVotes(uint64_t const pbft_round, blk_hash_t const& last_pbft_block_hash, size_t const sortition_threshold,
                                        uint64_t eligible_voter_count, std::function<bool(addr_t const&)> const& is_eligible) {
  // Cleanup votes for previous rounds
  cleanupVotes(pbft_round);

  // Track how many errant votes were found
  // and if sufficient number in the future we will
  // use this to trigger sync...
  uint64_t next_vote_with_different_prev_block_has_count = 0;

  std::vector<Vote> verified_votes;

  auto votes_to_verify = getAllVotes();
  for (auto const& v : votes_to_verify) {
    // vote verification
    addr_t voter_account_address = dev::toAddress(v.getVoter());
    // Check if the voter account is valid, malicious vote
    if (!is_eligible(voter_account_address)) {
      LOG(log_dg_) << "Account " << voter_account_address << " is not eligible to vote";
      continue;
    }
    if (voteValidation(last_pbft_block_hash, v, eligible_voter_count, sortition_threshold)) {
      verified_votes.emplace_back(v);
    } else if (v.getRound() == pbft_round && v.getType() == next_vote_type) {
      // We know that votes in our current round should reference our latest
      // PBFT chain block This is not immune to malacious attack!!!
      LOG(log_dg_) << "Next vote in current round " << pbft_round << " points to different block hash " << last_pbft_block_hash << " | vote: " << v
                   << " voter address: " << voter_account_address;
      next_vote_with_different_prev_block_has_count++;
    }
  }

  if (next_vote_with_different_prev_block_has_count > 0) {
    LOG(log_er_) << "Get votes found " << next_vote_with_different_prev_block_has_count << " next votes in round " << pbft_round
                 << " pointing to different previous pbft chain block hash";
  }

  return verified_votes;
}

std::string VoteManager::getJsonStr(std::vector<Vote> const& votes) {
  Json::Value ptroot;
  Json::Value ptvotes(Json::arrayValue);

  for (Vote const& v : votes) {
    Json::Value ptvote;
    ptvote["vote_hash"] = v.getHash().toString();
    ptvote["accounthash"] = v.getVoter().toString();
    ptvote["sortition_proof"] = v.getSortitionProof().toString();
    ptvote["vote_signature"] = v.getVoteSignature().toString();
    ptvote["blockhash"] = v.getBlockHash().toString();
    ptvote["type"] = v.getType();
    ptvote["round"] = Json::Value::UInt64(v.getRound());
    ptvote["step"] = Json::Value::UInt64(v.getStep());
    ptvotes.append(ptvote);
  }
  ptroot["votes"] = ptvotes;

  return ptroot.toStyledString();
}

// Return all votes in map unverified_votes_
std::vector<Vote> VoteManager::getAllVotes() {
  std::vector<Vote> votes;

  sharedLock_ lock(access_);
  for (auto const& round_votes : unverified_votes_) {
    std::transform(round_votes.second.begin(), round_votes.second.end(), std::back_inserter(votes),
                   [](std::pair<const taraxa::vote_hash_t, taraxa::Vote> const& v) { return v.second; });
  }
  return votes;
}

bool VoteManager::pbftBlockHasEnoughValidCertVotes(PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players,
                                                   size_t sortition_threshold, size_t pbft_2t_plus_1) const {
  if (pbft_block_and_votes.cert_votes.empty()) {
    LOG(log_er_) << "No any cert votes! The synced PBFT block comes from a "
                    "malicious player.";
    return false;
  }
  blk_hash_t pbft_chain_last_block_hash = pbft_chain_->getLastPbftBlockHash();
  std::vector<Vote> valid_votes;
  auto first_cert_vote_round = pbft_block_and_votes.cert_votes[0].getRound();
  for (auto const& v : pbft_block_and_votes.cert_votes) {
    // Any info is wrong that can determine the synced PBFT block comes from a
    // malicious player
    if (v.getType() != cert_vote_type) {
      LOG(log_er_) << "For PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << ", cert vote " << v.getHash() << " has wrong vote type "
                   << v.getType();
      break;
    } else if (v.getRound() != first_cert_vote_round) {
      LOG(log_er_) << "For PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << ", cert vote " << v.getHash()
                   << " has a different vote round " << v.getRound() << ", compare to first cert vote "
                   << pbft_block_and_votes.cert_votes[0].getHash() << " has vote round " << first_cert_vote_round;
      break;
    } else if (v.getStep() != 3) {
      LOG(log_er_) << "For PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << ", cert vote " << v.getHash() << " has wrong vote step "
                   << v.getStep();
      break;
    } else if (v.getBlockHash() != pbft_block_and_votes.pbft_blk->getBlockHash()) {
      LOG(log_er_) << "For PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << ", cert vote " << v.getHash()
                   << " has wrong vote block hash " << v.getBlockHash();
      break;
    }
    if (voteValidation(pbft_chain_last_block_hash, v, valid_sortition_players, sortition_threshold)) {
      valid_votes.emplace_back(v);
    } else {
      LOG(log_wr_) << "For PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << ", cert vote " << v.getHash() << " failed validation";
    }
  }
  if (valid_votes.size() < pbft_2t_plus_1) {
    LOG(log_er_) << "PBFT block " << pbft_block_and_votes.pbft_blk->getBlockHash() << " with " << pbft_block_and_votes.cert_votes.size()
                 << " cert votes. Has " << valid_votes.size() << " valid cert votes. 2t+1 is " << pbft_2t_plus_1 << ", Valid sortition players "
                 << valid_sortition_players << ", sortition threshold is " << sortition_threshold << ". The last block in pbft chain is "
                 << pbft_chain_last_block_hash;
  }
  return valid_votes.size() >= pbft_2t_plus_1;
}

}  // namespace taraxa
