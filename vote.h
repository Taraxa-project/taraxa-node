/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-04-23
 */

#ifndef VOTE_H
#define VOTE_H

#include "libdevcore/Log.h"
#include "libdevcrypto/Common.h"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"

#include <deque>

namespace taraxa {

class Vote {
 public:
  Vote() = default;
  Vote(public_t node_pk, sig_t signature, blk_hash_t blockhash,
       PbftVoteTypes type, uint64_t period, size_t step);
  Vote(stream& strm);
  ~Vote() {}

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  bool validateVote(std::pair<bal_t, bool>& vote_account_balance) const;

  sig_hash_t getHash() const;
  public_t getPublicKey() const;
  sig_t getSingature() const;
  blk_hash_t getBlockHash() const;
  PbftVoteTypes getType() const;
  uint64_t getPeriod() const;
  size_t getStep() const;

 private:
  public_t node_pk_;
  sig_t signature_;
  blk_hash_t blockhash_;
  PbftVoteTypes type_;
  uint64_t period_;
  size_t step_;

  mutable dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "VOTE")};
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "VOTE")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "VOTE")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "VOTE")};
};

class VoteQueue {
 public:
  VoteQueue() = default;
  ~VoteQueue() {}

  void clearQueue();

  size_t getSize();
  std::vector<Vote> getVotes(uint64_t period);
  std::string getJsonStr(std::vector<Vote>& votes);

  void placeVote(Vote const& vote);

  void placeVote(public_t const& node_pk, secret_t const& node_sk,
      blk_hash_t const& blockhash, PbftVoteTypes type, uint64_t period,
      size_t step);

 private:
  std::deque<Vote> vote_queue_;
};

}  // namespace taraxa

#endif  // VOTE_H
