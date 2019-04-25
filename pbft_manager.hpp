/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-04-23
 */

#ifndef PBFT_MANAGER_HPP
#define PBFT_MANAGER_HPP

#include <string>
#include <thread>

#include "libdevcore/Log.h"
#include "types.hpp"
#include "vote.h"

#define NULL_BLOCK "0"
#define NULL_BLOCK_HASH dev::sha3(NULL_BLOCK)
#define TWO_T_PLUS_ONE 3  // this is the 2t+1 value..
#define LAMBDA_ms 1000  // milliseconds
#define POLLING_INTERVAL_ms 100 // milliseconds...
#define MAX_STEPS 19

namespace taraxa {
class FullNode;
class VoteQueue;

enum PbftBlockTypes {
  pivot_block_type = 0,
  schedule_block_type,
  result_block
};

enum PbftVoteTypes {
  propose_vote_type = 0,
  next_vote_type,
  soft_vote_type,
  cert_vote_type
};

class PbftManager {
 public:
  PbftManager();
  ~PbftManager() { stop(); }
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  bool shouldSpeak(blk_hash_t const &blockhash, char type, int period,
                   int step);
  void start();
  void stop();
  void run();
  bool isActive() { return executor_ != nullptr; }

 private:
  size_t periodDeterminedFromVotes_(std::vector<Vote> &votes,
                                    size_t local_period);
  std::vector<Vote> getVotesOfTypeFromPeriod_(
      int vote_type,
      std::vector<Vote> &votes,
      size_t period,
      std::pair<blk_hash_t, bool> blockhash);
  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(std::vector<Vote> &votes);
  bool nullBlockNextVotedForPeriod_(std::vector<Vote> &votes, size_t period);
  std::vector<Vote> getVotesOfTypeFromVotesForPeriod_(
      int vote_type,
      std::vector<Vote> &votes,
      size_t period,
      std::pair<blk_hash_t, bool> blockhash);
  std::pair<blk_hash_t, bool> nextVotedBlockForPeriod_(
      std::vector<Vote> &votes, size_t period);
  void placeVoteIfCanSpeak_(blk_hash_t blockhash,
                           int vote_type,
                           size_t period,
                           size_t step,
                           bool override_sortition_check);
  std::pair<blk_hash_t, bool> softVotedBlockForPeriod_(std::vector<Vote> &votes,
                                                       size_t period);

  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<std::thread> executor_;
  std::shared_ptr<VoteQueue> vote_queue_;

  size_t pbft_period_ = 1;
  size_t pbft_step_ = 1;


  mutable dev::Logger log_sil_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFT_MGR")};
  mutable dev::Logger log_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFT_MGR")};
  mutable dev::Logger log_war_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFT_MGR")};
  mutable dev::Logger log_inf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_MGR")};
  mutable dev::Logger log_deb_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFT_MGR")};
  mutable dev::Logger log_tra_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFT_MGR")};
};

}  // namespace taraxa

#endif  // PBFT_MANAGER_H
