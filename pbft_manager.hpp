/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-01
 */

#ifndef PBFT_MANAGER_HPP
#define PBFT_MANAGER_HPP

#include <string>
#include <thread>

#include "config.hpp"
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "SimpleDBFace.h"
#include "taraxa_capability.h"
#include "types.hpp"
#include "vote.h"

#define NULL_BLOCK_HASH blk_hash_t(0)
#define TWO_T_PLUS_ONE 3  // this is the 2t+1 value.. // TODO: need to change the value
#define LAMBDA_ms 1000  // milliseconds
#define POLLING_INTERVAL_ms 100 // milliseconds...
#define MAX_STEPS 19
#undef LAMBDA_ms // undef for test TODO: need remove later

namespace taraxa {
class FullNode;
class VoteQueue;

class PbftManager {
 public:
  PbftManager();
  PbftManager(PbftManagerConfig const &config);
  ~PbftManager() { stop(); }
  void setFullNode(std::shared_ptr<FullNode> node);
  bool shouldSpeak(blk_hash_t const &blockhash, PbftVoteTypes type,
                   uint64_t period, size_t step);
  void start();
  void stop();
  void run();
  bool isActive() { return executor_ != nullptr; }

  // only for test
  u_long getLambdaMs() const { return LAMBDA_ms; }
  void setPbftPeriod(uint64_t const pbft_period) { pbft_period_ = pbft_period; }
  void setPbftStep(size_t const pbft_step) { pbft_step_ = pbft_step; }
  uint64_t getPbftPeriod() const { return pbft_period_; }
  size_t getPbftStep() const { return pbft_step_; }

 private:
  size_t periodDeterminedFromVotes_(std::vector<Vote> &votes,
                                    uint64_t local_period);
  std::vector<Vote> getVotesOfTypeFromPeriod_(
      PbftVoteTypes vote_type,
      std::vector<Vote> &votes,
      uint64_t period,
      std::pair<blk_hash_t, bool> blockhash);
  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(std::vector<Vote> &votes);
  bool nullBlockNextVotedForPeriod_(std::vector<Vote> &votes, uint64_t period);
  std::vector<Vote> getVotesOfTypeFromVotesForPeriod_(
      PbftVoteTypes vote_type,
      std::vector<Vote> &votes,
      uint64_t period,
      std::pair<blk_hash_t, bool> blockhash);
  std::pair<blk_hash_t, bool> nextVotedBlockForPeriod_(
      std::vector<Vote> &votes, uint64_t period);
  void placeVoteIfCanSpeak_(blk_hash_t blockhash,
                            PbftVoteTypes vote_type,
                            uint64_t period,
                            size_t step,
                            bool override_sortition_check);
  void broadcastPbftVote_(blk_hash_t &blockhash, PbftVoteTypes vote_type,
      uint64_t period, size_t step);
  std::pair<blk_hash_t, bool> softVotedBlockForPeriod_(std::vector<Vote> &votes,
                                                       uint64_t period);
  std::pair<blk_hash_t, bool> proposeMyPbftBlock_();
  std::pair<blk_hash_t, bool> identifyLeaderBlock_();
  bool pushPbftBlockIntoChain_(uint64_t period,
                               blk_hash_t const& cert_vote_block_hash);
  bool updatePbftChainDB_(PbftBlock const& pbft_block);
  bool checkPbftBlockValid_(blk_hash_t const& block_hash);

  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<std::thread> executor_;
  std::shared_ptr<VoteQueue> vote_queue_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<SimpleDBFace> db_votes_;
  std::shared_ptr<SimpleDBFace> db_pbftchain_;
  std::shared_ptr<TaraxaCapability> capability_;

  uint64_t pbft_period_ = 1;
  size_t pbft_step_ = 1;

  u_long LAMBDA_ms; // Only for test TODO: need remove later

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
