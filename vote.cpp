#include "vote.h"
#include "full_node.hpp"
#include "libdevcore/SHA3.h"
#include "pbft_manager.hpp"
#include "sortition.h"

#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace taraxa {
VrfSortition::VrfSortition(bytes const& b) {
  dev::RLP const rlp(b);
  if (!rlp.isList())
    throw std::invalid_argument("VrfSortition RLP must be a list");
  pk = rlp[0].toHash<vrf_pk_t>();
  blk = rlp[1].toHash<blk_hash_t>();
  type = PbftVoteTypes(rlp[2].toInt<uint>());
  round = rlp[3].toInt<uint64_t>();
  step = rlp[4].toInt<size_t>();
  proof = rlp[5].toHash<vrf_proof_t>();
  output = rlp[6].toHash<vrf_output_t>();
}
bytes VrfSortition::getRlpBytes() const {
  dev::RLPStream s;
  s.appendList(7);
  s << pk;
  s << blk;
  s << type;
  s << round;
  s << step;
  s << proof;
  s << output;
  return s.out();
}
/*
 * Sortition return true:
 * CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
 * i.e., CREDENTIAL * VALID PLAYERS <= SORTITION THRESHOLD * SIGNATURE_HASH_MAX
 * otherwise return false
 */
bool VrfSortition::canSpeak(size_t threshold, size_t valid_players) const {
  uint1024_t left = (uint1024_t)((uint512_t)output) * valid_players;
  uint1024_t right = (uint1024_t)max512bits * threshold;
  return left <= right;
}

Vote::Vote(bytes const& b) {
  dev::RLP const rlp(b);
  if (!rlp.isList())
    throw std::invalid_argument("transaction RLP must be a list");
  vote_hash_ = rlp[0].toHash<vote_hash_t>();
  node_pk_ = rlp[1].toHash<public_t>();
  blockhash_ = rlp[2].toHash<blk_hash_t>();
  vote_signatue_ = rlp[3].toHash<sig_t>();
  vrf_sortition_ = VrfSortition(rlp[4].toBytes());
}

bytes Vote::rlp() const {
  dev::RLPStream s;
  streamRLP_(s);
  return s.out();
}

void Vote::streamRLP_(dev::RLPStream& strm) const {
  strm.appendList(5);
  strm << vote_hash_;
  strm << node_pk_;
  strm << blockhash_;
  strm << vote_signatue_;
  strm << vrf_sortition_.getRlpBytes();
}

vote_hash_t Vote::getHash() const { return vote_hash_; }

public_t Vote::getPublicKey() const { return node_pk_; }

auto Vote::getSortitionProof() const { return vrf_sortition_.proof; }

sig_t Vote::getVoteSignature() const { return vote_signatue_; }

blk_hash_t Vote::getBlockHash() const { return blockhash_; }

PbftVoteTypes Vote::getType() const { return vrf_sortition_.type; }

uint64_t Vote::getRound() const { return vrf_sortition_.round; }

size_t Vote::getStep() const { return vrf_sortition_.step; }

// Vote Manager
void VoteManager::setFullNode(std::shared_ptr<taraxa::FullNode> full_node) {
  node_ = full_node;
  pbft_chain_ = full_node->getPbftChain();
  pbft_mgr_ = full_node->getPbftManager();
}

// Problem??
sig_t VoteManager::signVote(secret_t const& node_sk,
                            taraxa::blk_hash_t const& block_hash,
                            taraxa::PbftVoteTypes type, uint64_t round,
                            size_t step) {
  std::string message = block_hash.toString() + std::to_string(type) +
                        std::to_string(round) + std::to_string(step);
  // sign message
  return dev::sign(node_sk, hash_(message));
}

bool VoteManager::voteValidation(taraxa::blk_hash_t const& last_pbft_block_hash,
                                 taraxa::Vote const& vote,
                                 size_t valid_sortition_players,
                                 size_t sortition_threshold) const {
  if (last_pbft_block_hash != vote.getVrfSorition().blk) {
    LOG(log_war_) << "Last pbft block hash does not match "
                  << last_pbft_block_hash;
    return false;
  }
  if (!vote.verifyVote()) {
    LOG(log_war_) << "Invalid vote signature, vote hash " << vote.getHash();
    return false;
  }
  if (!vote.verifyCanSpeak(sortition_threshold, valid_sortition_players)) {
    LOG(log_war_) << "Vote sortition failed, sortition proof "
                  << vote.getSortitionProof();
    return false;
  }
  return true;
}

bool VoteManager::addVote(taraxa::Vote const& vote) {
  uint64_t pbft_round = vote.getRound();
  auto hash = vote.getHash();

  {
    upgradableLock_ lock(access_);
    std::map<uint64_t, std::map<vote_hash_t, Vote>>::const_iterator
        found_round = unverified_votes_.find(pbft_round);
    if (found_round != unverified_votes_.end()) {
      std::map<vote_hash_t, Vote>::const_iterator found_vote =
          found_round->second.find(hash);
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
  LOG(log_deb_) << "Add vote " << hash << ", block hash " << vote.getBlockHash()
                << ", vote type " << vote.getType() << ", in round "
                << pbft_round << ", for step " << vote.getStep();
  return true;
}

// cleanup votes < pbft_round
void VoteManager::cleanupVotes(uint64_t pbft_round) {
  upgradableLock_ lock(access_);
  std::map<uint64_t, std::map<vote_hash_t, Vote>>::iterator it =
      unverified_votes_.begin();

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
  for (it = unverified_votes_.begin(); it != unverified_votes_.end(); it++) {
    size += it->second.size();
  }

  return size;
}

// Return all verified votes >= pbft_round
std::vector<Vote> VoteManager::getVotes(uint64_t pbft_round,
                                        size_t valid_sortition_players) {
  cleanupVotes(pbft_round);

  std::vector<Vote> verified_votes;

  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Vote Manager full node weak pointer empty";
    return verified_votes;
  }

  blk_hash_t last_pbft_block_hash =
      pbft_mgr_->getLastPbftBlockHashAtStartOfRound();
  size_t sortition_threshold = pbft_mgr_->getSortitionThreshold();

  std::map<uint64_t, std::vector<Vote>>::const_iterator it;
  auto votes_to_verify = getAllVotes();
  for (auto const& v : votes_to_verify) {
    // vote verification
    // TODO: or search in PBFT sortition_account_balance_table?
    addr_t vote_address = dev::toAddress(v.getPublicKey());
    std::pair<val_t, bool> account_balance =
        full_node->getBalance(vote_address);
    if (!account_balance.second) {
      // New node join, it doesn't have other nodes info.
      // Wait unit sync PBFT chain with peers, and execute to get states.
      LOG(log_deb_) << "Cannot find the vote account balance. vote hash: "
                    << v.getHash() << " vote address: " << vote_address;
      continue;
    }
    if (voteValidation(last_pbft_block_hash, v, valid_sortition_players,
                       sortition_threshold)) {
      verified_votes.emplace_back(v);
    }
  }

  return verified_votes;
}

// Return all verified votes >= pbft_round
std::vector<Vote> VoteManager::getVotes(uint64_t pbft_round,
                                        size_t valid_sortition_players,
                                        bool& sync_peers_pbft_chain) {
  cleanupVotes(pbft_round);

  // Should be sure we always write a value to this pointer...
  sync_peers_pbft_chain = false;

  std::vector<Vote> verified_votes;

  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Vote Manager full node weak pointer empty";
    return verified_votes;
  }

  blk_hash_t last_pbft_block_hash =
      pbft_mgr_->getLastPbftBlockHashAtStartOfRound();
  size_t sortition_threshold = pbft_mgr_->getSortitionThreshold();

  std::map<uint64_t, std::vector<Vote>>::const_iterator it;
  auto votes_to_verify = getAllVotes();
  for (auto const& v : votes_to_verify) {
    // vote verification
    // TODO: or search in PBFT sortition_account_balance_table?
    addr_t vote_address = dev::toAddress(v.getPublicKey());
    std::pair<val_t, bool> account_balance =
        full_node->getBalance(vote_address);
    if (!account_balance.second) {
      // New node join, it doesn't have other nodes info.
      // Wait unit sync PBFT chain with peers, and execute to get states.
      LOG(log_deb_) << "Cannot find the vote account balance. vote hash: "
                    << v.getHash() << " vote address: " << vote_address;
      sync_peers_pbft_chain = true;
      continue;
    }
    if (voteValidation(last_pbft_block_hash, v, valid_sortition_players,
                       sortition_threshold)) {
      verified_votes.emplace_back(v);
    } else if (v.getRound() == pbft_round + 1 &&
               v.getType() == next_vote_type) {
      // We know that votes in our current round should reference our latest
      // PBFT chain block This is not immune to malacious attack!!!
      LOG(log_deb_) << "Next vote in current round " << pbft_round + 1
                    << " points to different block hash "
                    << last_pbft_block_hash << " | vote hash: " << v.getHash()
                    << " vote address: " << vote_address;
      sync_peers_pbft_chain = true;
    }
  }
  return verified_votes;
}

std::string VoteManager::getJsonStr(std::vector<Vote>& votes) {
  using boost::property_tree::ptree;
  ptree ptroot;
  ptree ptvotes;

  for (Vote const& v : votes) {
    ptree ptvote;
    ptvote.put("vote_hash", v.getHash());
    ptvote.put("accounthash", v.getPublicKey());
    ptvote.put("sortition_proof", v.getSortitionProof());
    ptvote.put("vote_signature", v.getVoteSignature());
    ptvote.put("blockhash", v.getBlockHash());
    ptvote.put("type", v.getType());
    ptvote.put("round", v.getRound());
    ptvote.put("step", v.getStep());
    ptvotes.push_back(std::make_pair("", ptvote));
  }
  ptroot.add_child("votes", ptvotes);

  std::stringstream output;
  boost::property_tree::write_json(output, ptroot);

  return output.str();
}

// Return all votes in map unverified_votes_
std::vector<Vote> VoteManager::getAllVotes() {
  std::vector<Vote> votes;

  sharedLock_ lock(access_);
  for (auto const& round_votes : unverified_votes_) {
    for (auto const& v : round_votes.second) {
      votes.emplace_back(v.second);
    }
  }

  return votes;
}

bool VoteManager::pbftBlockHasEnoughValidCertVotes(
    PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players,
    size_t sortition_threshold, size_t pbft_2t_plus_1) const {
  blk_hash_t pbft_chain_last_block_hash = pbft_chain_->getLastPbftBlockHash();
  std::vector<Vote> valid_votes;
  for (auto const& v : pbft_block_and_votes.cert_votes) {
    if (v.getType() != cert_vote_type) {
      LOG(log_war_) << "For PBFT block "
                    << pbft_block_and_votes.pbft_blk.getBlockHash()
                    << ", cert vote " << v.getHash() << " has wrong vote type "
                    << v.getType();
      continue;
    } else if (v.getStep() != 3) {
      LOG(log_war_) << "For PBFT block "
                    << pbft_block_and_votes.pbft_blk.getBlockHash()
                    << ", cert vote " << v.getHash() << " has wrong vote step "
                    << v.getStep();
      continue;
    } else if (v.getBlockHash() !=
               pbft_block_and_votes.pbft_blk.getBlockHash()) {
      LOG(log_war_) << "For PBFT block "
                    << pbft_block_and_votes.pbft_blk.getBlockHash()
                    << ", cert vote " << v.getHash()
                    << " has wrong vote block hash " << v.getBlockHash();
      continue;
    }
    if (voteValidation(pbft_chain_last_block_hash, v, valid_sortition_players,
                       sortition_threshold)) {
      valid_votes.emplace_back(v);
    } else {
      LOG(log_war_) << "For PBFT block "
                    << pbft_block_and_votes.pbft_blk.getBlockHash()
                    << ", cert vote " << v.getHash() << " failed validation";
    }
  }
  if (valid_votes.size() < pbft_2t_plus_1) {
    LOG(log_err_) << "PBFT block "
                  << pbft_block_and_votes.pbft_blk.getBlockHash() << " with "
                  << pbft_block_and_votes.cert_votes.size()
                  << " cert votes. Has " << valid_votes.size()
                  << " valid cert votes. 2t+1 is " << pbft_2t_plus_1
                  << ", Valid sortition players " << valid_sortition_players
                  << ", sortition threshold is " << sortition_threshold
                  << ". The last block in pbft chain is "
                  << pbft_chain_last_block_hash;
  }
  return valid_votes.size() >= pbft_2t_plus_1;
}

vote_hash_t VoteManager::hash_(std::string const& str) const {
  return dev::sha3(str);
}

}  // namespace taraxa
