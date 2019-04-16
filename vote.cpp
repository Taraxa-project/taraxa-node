/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by:
 * @Last Modified time:
 */

#include "vote.h"

#include "libdevcore/SHA3.h"

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

void VoteQueue::placeVote(public_t node_pk,
                          dev::Signature signature,
                          blk_hash_t blockhash,
                          char type,
                          int period,
                          int step) {
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
    if (it->period_ < period) {
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
    ptvote.put("blockhash", v.blockhash_);
    ptvote.put("accounthash", v.node_pk_);
    ptvote.put("type", v.type_);
    ptvote.put("period", v.period_);
    ptvote.put("step", v.step_);
    ptvotes.push_back(std::make_pair("", ptvote));
  }
  ptroot.add_child("votes", ptvotes);

  std::stringstream output;
  boost::property_tree::write_json(output, ptroot);

  return output.str();
}

} // namespace taraxa
