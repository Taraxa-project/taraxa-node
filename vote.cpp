/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by:
 * @Last Modified time:
 */

#include "vote.h"

#include "libdevcore/SHA3.h"
#include "sortition.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace taraxa {

Vote::Vote(public_t node_pk,
           dev::Signature signature,
           blk_hash_t blockhash,
           char type,
           int period,
           int step) : node_pk_(node_pk),
                       signature_(signature),
                       blockhash_(blockhash),
                       type_(type),
                       period_(period),
                       step_(step) {
}

Vote::Vote(taraxa::stream &strm) {
  deserialize(strm);
}

bool Vote::serialize(stream &strm) const {
  bool ok = true;

  ok &= write(strm, node_pk_);
  ok &= write(strm, signature_);
  ok &= write(strm, blockhash_);
  ok &= write(strm, type_);
  ok &= write(strm, period_);
  ok &= write(strm, step_);
  assert(ok);

  return ok;
}

bool Vote::deserialize(stream &strm) {
  bool ok = true;

  ok &= read(strm, node_pk_);
  ok &= read(strm, signature_);
  ok &= read(strm, blockhash_);
  ok &= read(strm, type_);
  ok &= read(strm, period_);
  ok &= read(strm, step_);
  assert(ok);

  return ok;
}

sig_hash_t Vote::getHash() {
  return dev::sha3(signature_);
}

public_t Vote::getPublicKey() {
  return node_pk_;
}

dev::Signature Vote::getSingature() {
  return signature_;
}

blk_hash_t Vote::getBlockHash() {
  return blockhash_;
}

char Vote::getType() {
  return type_;
}

int Vote::getPeriod() {
  return period_;
}

int Vote::getStep() {
  return step_;
}

bool Vote::validateVote(std::pair<bal_t, bool> vote_account_balance) {
  if (!vote_account_balance.second) {
    LOG(log_er_) << "Invalid vote account balance" << std::endl;
    return false;
  }
  // verify signature
  std::string vote_message = blockhash_.toString() +
                             std::to_string(type_) +
                             std::to_string(period_) +
                             std::to_string(step_);
  if (!dev::verify(node_pk_, signature_, dev::sha3(vote_message))) {
    LOG(log_er_) << "Invalid vote signature: " << signature_ << std::endl;
    return false;
  }
  // verify sortition
  std::string vote_signature_hash = taraxa::hashSignature(signature_);
  if (taraxa::sortition(vote_signature_hash, vote_account_balance.first)) {
    return true;
  }
  return false;
}

void VoteQueue::placeVote(public_t node_pk,
                          secret_t node_sk,
                          blk_hash_t blockhash,
                          char type,
                          int period,
                          int step) {
  std::string message = blockhash.toString() +
                        std::to_string(type) +
                        std::to_string(period) +
                        std::to_string(step);
  // sign message
  dev::Signature signature = dev::sign(node_sk, dev::sha3(message));

  Vote vote(node_pk, signature, blockhash, type, period, step);
  placeVote(vote);
}

void VoteQueue::placeVote(taraxa::Vote vote) {
  vote_queue.push_back(vote);
}

std::vector<Vote> VoteQueue::getVotes(int period) {
  std::vector<Vote> votes;
  std::deque<Vote>::iterator it = vote_queue.begin();

  while (it != vote_queue.end()) {
    if (it->getPeriod() < period) {
      vote_queue.erase(it);
      continue;
    }
    votes.push_back(*it++);
  }

  return votes;
}

std::string VoteQueue::getJsonStr(std::vector<Vote> votes) {
  using boost::property_tree::ptree;
  ptree ptroot;
  ptree ptvotes;

  for (Vote v : votes) {
    ptree ptvote;
    ptvote.put("blockhash", v.getBlockHash());
    ptvote.put("accounthash", v.getPublicKey());
    ptvote.put("type", v.getType());
    ptvote.put("period", v.getPeriod());
    ptvote.put("step", v.getStep());
    ptvotes.push_back(std::make_pair("", ptvote));
  }
  ptroot.add_child("votes", ptvotes);

  std::stringstream output;
  boost::property_tree::write_json(output, ptroot);

  return output.str();
}

} // namespace taraxa
