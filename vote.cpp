/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-04-23
 */

#include "vote.h"

#include "libdevcore/SHA3.h"
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
    // TODO: 1. Get vote too fast, and PBFT chain has not add the new block yet.
    //  2. When new node join, the new node doesn't have the last pbft block to
    //  verify.
    //  Need to save the valid votes
    LOG(log_war_) << "Get it too fast! Invalid sortition signature: "
                  << sortition_signature << " vote hash " << vote.getHash();
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

vote_hash_t VoteManager::hash_(std::string const& str) const {
  return dev::sha3(str);
}

// Vote Queue
void VoteQueue::clearQueue() { vote_queue_.clear(); }

size_t VoteQueue::getSize() { return vote_queue_.size(); }

std::vector<Vote> VoteQueue::getVotes(uint64_t round) {
  std::vector<Vote> votes;
  std::deque<Vote>::iterator it = vote_queue_.begin();

  while (it != vote_queue_.end()) {
    if (it->getRound() < round) {
      it = vote_queue_.erase(it);
    } else {
      votes.emplace_back(*it++);
    }
  }

  return votes;
}

std::string VoteQueue::getJsonStr(std::vector<Vote>& votes) {
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

void VoteQueue::pushBackVote(taraxa::Vote const& vote) {
  vote_queue_.emplace_back(vote);
}

}  // namespace taraxa
