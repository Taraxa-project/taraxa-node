#include "vote/vote.hpp"

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

Vote::Vote(const blk_hash_t& block_hash) : block_hash_(block_hash) {}

void Vote::signVote(const secret_t& node_sk) {
  vote_signature_ = dev::sign(node_sk, sha3(false));
  cached_voter_ = dev::toPublic(node_sk);
  cached_voter_addr_ = dev::toAddress(cached_voter_);
}

const vote_hash_t& Vote::getHash() const {
  if (!vote_hash_) vote_hash_ = sha3(true);
  return vote_hash_;
}

const public_t& Vote::getVoter() const {
  if (!cached_voter_) cached_voter_ = dev::recover(vote_signature_, sha3(false));
  return cached_voter_;
}

const addr_t& Vote::getVoterAddr() const {
  if (!cached_voter_addr_) cached_voter_addr_ = dev::toAddress(getVoter());
  return cached_voter_addr_;
}

const sig_t& Vote::getVoteSignature() const { return vote_signature_; }

const blk_hash_t& Vote::getBlockHash() const { return block_hash_; }

bool Vote::verifyVote() const {
  auto pk = getVoter();
  return !pk.isZero();  // recoverd public key means that it was verified
}

}  // namespace taraxa