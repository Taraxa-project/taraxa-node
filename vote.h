/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-11
 * @Last Modified by:
 * @Last Modified time:
 */

#ifndef VOTE_H
#define VOTE_H

#include "libdevcrypto/Common.h"
#include "types.hpp"
#include "util.hpp"

#include <deque>

namespace taraxa {

class Vote {
 public:
  Vote() = default;
  Vote(public_t node_pk,
       dev::Signature signature,
       blk_hash_t blockhash,
       char type,
       int period,
       int step);
  Vote(stream &strm);
  ~Vote() {}

  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);

  sig_hash_t getHash();

  public_t node_pk_;
  dev::Signature signature_;
  blk_hash_t blockhash_;
  char type_;
  int period_;
  int step_;
};

class VoteQueue {
 public:
  VoteQueue() = default;
  ~VoteQueue() {}

  std::vector<Vote> getVotes(int period);

  void placeVote(Vote vote);

  void placeVote(public_t node_pk,
                 dev::Signature signature,
                 blk_hash_t blockhash,
                 char type,
                 int period,
                 int step);

  std::string getJsonStr(std::vector<Vote> votes);

 private:
  std::deque<Vote> vote_queue;
};

} // namespace taraxa

#endif  // VOTE_H
