//
// Created by Qi Gao on 2019-04-10.
//

#ifndef VOTE_H
#define VOTE_H

#include "types.hpp"

#include <deque>

namespace taraxa {

class Vote {
 public:
  Vote(blk_hash_t blockhash,
       char type,
       int period,
       int step,
       public_t node_pk);
  ~Vote() {}

  blk_hash_t blockhash_;
  char type_;
  int period_;
  int step_;
  public_t node_pk_;
};

class VoteQueue {
 public:
  VoteQueue() = default;
  ~VoteQueue() {}

  std::vector<Vote> getVotes(int period);

  void placeVote(Vote vote);

  void placeVote(blk_hash_t blockhash,
                 char type,
                 int period,
                 int step,
                 public_t node_pk);

  std::string getJsonStr(std::vector<Vote> votes);

 private:
  std::deque<Vote> vote_queue;
};

} // namespace taraxa

#endif  // VOTE_H
