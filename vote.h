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
#include "types.hpp"
#include "util.hpp"

#include <deque>

namespace taraxa {

class Vote {
 public:
  Vote() = default;
  Vote(public_t node_pk, dev::Signature signature, blk_hash_t blockhash,
       char type, int period, int step);
  Vote(stream &strm);
  ~Vote() {}

  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  bool validateVote(std::pair<bal_t, bool> &vote_account_balance) const;

  sig_hash_t getHash() const;
  public_t getPublicKey() const;
  dev::Signature getSingature() const;
  blk_hash_t getBlockHash() const;
  char getType() const;
  size_t getPeriod() const;
  size_t getStep() const;

 private:
  public_t node_pk_;
  dev::Signature signature_;
  blk_hash_t blockhash_;
  char type_;
  size_t period_;
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
  std::vector<Vote> getVotes(int period);
  std::string getJsonStr(std::vector<Vote> &votes);

  void placeVote(Vote const &vote);

  void placeVote(public_t const &node_pk, secret_t const &node_sk,
                 blk_hash_t const &blockhash, char type, int period, int step);

 private:
  std::deque<Vote> vote_queue_;
};

}  // namespace taraxa

#endif  // VOTE_H
