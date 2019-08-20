/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-07-26
 */

#include "vote.h"

#include "full_node.hpp"
#include "libdevcore/SHA3.h"
#include "pbft_manager.hpp"
#include "sortition.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace taraxa {

// Vote
Vote::Vote(public_t const& node_pk, sig_t const& sortition_signaure,
           sig_t const& vote_signature, blk_hash_t const& blockhash,
           PbftVoteTypes type, uint64_t round, size_t step)
    : node_pk_(node_pk),
      sortition_signature_(sortition_signaure),
      vote_signatue_(vote_signature),
      blockhash_(blockhash),
      type_(type),
      round_(round),
      step_(step) {
  dev::RLPStream s;
  streamRLP_(s);
  vote_hash_ = dev::sha3(s.out());
}

Vote::Vote(taraxa::stream& strm) { deserialize(strm); }

bool Vote::serialize(stream& strm) const {
  bool ok = true;

  ok &= write(strm, vote_hash_);
  ok &= write(strm, node_pk_);
  ok &= write(strm, sortition_signature_);
  ok &= write(strm, vote_signatue_);
  ok &= write(strm, blockhash_);
  ok &= write(strm, type_);
  ok &= write(strm, round_);
  ok &= write(strm, step_);
  assert(ok);

  return ok;
}

bool Vote::deserialize(stream& strm) {
  bool ok = true;

  ok &= read(strm, vote_hash_);
  ok &= read(strm, node_pk_);
  ok &= read(strm, sortition_signature_);
  ok &= read(strm, vote_signatue_);
  ok &= read(strm, blockhash_);
  ok &= read(strm, type_);
  ok &= read(strm, round_);
  ok &= read(strm, step_);
  assert(ok);

  return ok;
}

void Vote::streamRLP_(dev::RLPStream& strm) const {
  strm << vote_hash_;
  strm << node_pk_;
  strm << sortition_signature_;
  strm << vote_signatue_;
  strm << blockhash_;
  strm << type_;
  strm << round_;
  strm << step_;
}

vote_hash_t Vote::getHash() const { return vote_hash_; }

public_t Vote::getPublicKey() const { return node_pk_; }

sig_t Vote::getSortitionSignature() const { return sortition_signature_; }

sig_t Vote::getVoteSignature() const { return vote_signatue_; }

blk_hash_t Vote::getBlockHash() const { return blockhash_; }

PbftVoteTypes Vote::getType() const { return type_; }

uint64_t Vote::getRound() const { return round_; }

size_t Vote::getStep() const { return step_; }

// Vote Manager
void VoteManager::setFullNode(std::shared_ptr<taraxa::FullNode> node) {
  node_ = node;

  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node is not available in Vote Manager";
    return;
  }
  pbft_chain_ = full_node->getPbftChain();
  pbft_mgr_ = full_node->getPbftManager();
}

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
                                 taraxa::val_t& account_balance,
                                 size_t sortition_threshold) const {
  PbftVoteTypes type = vote.getType();
  uint64_t round = vote.getRound();
  size_t step = vote.getStep();
  // verify vote signature
  std::string const vote_message = vote.getBlockHash().toString() +
                                   std::to_string(type) +
                                   std::to_string(round) + std::to_string(step);
  public_t public_key = vote.getPublicKey();
  sig_t vote_signature = vote.getVoteSignature();
  if (!dev::verify(public_key, vote_signature, hash_(vote_message))) {
    LOG(log_war_) << "Invalid vote signature " << vote_signature
                  << " vote hash " << vote.getHash();
    return false;
  }

  // verify sortition signature
  std::string const sortition_message =
      last_pbft_block_hash.toString() + std::to_string(type) +
      std::to_string(round) + std::to_string(step);
  sig_t sortition_signature = vote.getSortitionSignature();
  if (!dev::verify(public_key, sortition_signature, hash_(sortition_message))) {
    // 1. PBFT chain has not have the newest PBFT block yet
    // 2. When new node join, the new node doesn't have the lastest pbft block
    // to verify the vote.
    // Return false, will not effect PBFT consensus
    // if the vote is valid, will count when node has the relative pbft block
    // if the vote is invalid, will remove when pass the pbft round
    LOG(log_tra_) << "Haven't have the newest PBFT block to verify the vote "
                  << "sortition signature: " << sortition_signature
                  << " vote hash " << vote.getHash();
    return false;
  }

  // verify sortition
  std::string sortition_signature_hash =
      taraxa::hashSignature(sortition_signature);
  if (!taraxa::sortition(sortition_signature_hash, account_balance,
                         sortition_threshold)) {
    LOG(log_war_) << "Vote sortition failed, sortition signature "
                  << sortition_signature;
    return false;
  }

  return true;
}

bool VoteManager::isKnownVote(uint64_t pbft_round,
                              vote_hash_t const& vote_hash) const {
  sharedLock_ lock(access_);

  std::map<uint64_t, std::vector<Vote>>::const_iterator found =
      unverified_votes_.find(pbft_round);
  if (found == unverified_votes_.end()) {
    return false;
  }
  for (Vote const& v : found->second) {
    if (v.getHash() == vote_hash) {
      return true;
    }
  }
  return false;
}

void VoteManager::addVote(taraxa::Vote const& vote) {
  uint64_t pbft_round = vote.getRound();

  upgradableLock_ lock(access_);
  if (unverified_votes_.find(pbft_round) != unverified_votes_.end()) {
    upgradeLock_ locked(lock);
    unverified_votes_[pbft_round].emplace_back(vote);
  } else {
    std::vector<Vote> votes{vote};
    upgradeLock_ locked(lock);
    unverified_votes_[pbft_round] = votes;
  }
}

// cleanup votes < pbft_round
void VoteManager::cleanupVotes(uint64_t pbft_round) {
  upgradableLock_ lock(access_);
  std::map<uint64_t, std::vector<Vote>>::iterator it =
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
  std::map<uint64_t, std::vector<Vote>>::const_iterator it;
  for (it = unverified_votes_.begin(); it != unverified_votes_.end(); it++) {
    size += it->second.size();
  }

  return size;
}

// Return all votes >= pbft_round
std::vector<Vote> VoteManager::getVotes(uint64_t pbft_round) {
  cleanupVotes(pbft_round);

  std::vector<Vote> verified_votes;
  bool sync_peers_pbft_chain = false;

  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Vote Manager full node weak pointer empty";
    return verified_votes;
  }

  blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  size_t sortition_threshold = pbft_mgr_->getSortitionThreshold();

  std::map<uint64_t, std::vector<Vote>>::const_iterator it;
  sharedLock_ lock(access_);
  for (it = unverified_votes_.begin(); it != unverified_votes_.end(); it++) {
    for (auto const& v : it->second) {
      // vote verification
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
      if (voteValidation(last_pbft_block_hash, v, account_balance.first,
                         sortition_threshold)) {
        verified_votes.emplace_back(v);
      }
    }
  }

  // TODO: Temporary fix for node out of sync with peers, since there is no
  //  malicious votes for testnet1.0
  if (sync_peers_pbft_chain) {
    full_node->getPbftManager()->syncPbftChainFromPeers();
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
    ptvote.put("sortition_signature", v.getSortitionSignature());
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

vote_hash_t VoteManager::hash_(std::string const& str) const {
  return dev::sha3(str);
}

}  // namespace taraxa
