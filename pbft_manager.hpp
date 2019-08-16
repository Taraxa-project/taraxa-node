/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-07-25
 */

#ifndef PBFT_MANAGER_HPP
#define PBFT_MANAGER_HPP

#include <string>
#include <thread>

#include "SimpleDBFace.h"
#include "config.hpp"
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "taraxa_capability.h"
#include "types.hpp"
#include "vote.h"

#define NULL_BLOCK_HASH blk_hash_t(0)
#define LAMBDA_ms 1000           // milliseconds
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 19
#define COMMITTEE_SIZE 3  // TODO: The value for local test, need to change
#define VALID_SORTITION_COINS 10000  // TODO: the value may change later
#undef LAMBDA_ms                     // TODO: undef for test, need remove later
#undef COMMITTEE_SIZE                // TODO: undef for test, need remove later
#undef VALID_SORTITION_COINS  // // TODO: undef for test, need remove later

namespace taraxa {
class FullNode;

class PbftManager {
 public:
  PbftManager(std::string const &genesis);
  PbftManager(std::vector<uint> const &params, std::string const &genesis);
  ~PbftManager() {
    if (!stopped_) {
      stop();
    }
  }

  void setFullNode(std::shared_ptr<FullNode> node);
  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step);
  void start();
  void stop();
  void run();
  bool isActive() { return daemon_ != nullptr; }

  size_t getSortitionThreshold() const { return sortition_threshold_; }
  void setSortitionThreshold(size_t const sortition_threshold) {
    sortition_threshold_ = sortition_threshold;
  }
  void setTwoTPlusOne(size_t const two_t_plus_one) {
    TWO_T_PLUS_ONE = two_t_plus_one;
  }

  // only for test
  void setPbftRound(uint64_t const pbft_round) { pbft_round_ = pbft_round; }
  void setPbftStep(size_t const pbft_step) { pbft_step_ = pbft_step; }
  uint64_t getPbftRound() const { return pbft_round_; }
  size_t getPbftStep() const { return pbft_step_; }

  std::unordered_map<addr_t, val_t> sortition_account_balance_table;
  u_long LAMBDA_ms;                // TODO: Only for test, need remove later
  size_t COMMITTEE_SIZE;           // TODO: Only for test, need remove later
  uint64_t VALID_SORTITION_COINS;  // TODO: Only for test, need remove later

 private:
  size_t roundDeterminedFromVotes_(std::vector<Vote> &votes,
                                   uint64_t local_round);

  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(std::vector<Vote> &votes);

  bool nullBlockNextVotedForRound_(std::vector<Vote> &votes, uint64_t round);

  std::vector<Vote> getVotesOfTypeFromVotesForRound_(
      PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
      std::pair<blk_hash_t, bool> blockhash);

  std::pair<blk_hash_t, bool> nextVotedBlockForRound_(std::vector<Vote> &votes,
                                                      uint64_t round);

  void placeVote_(blk_hash_t const &blockhash, PbftVoteTypes vote_type,
                  uint64_t round, size_t step);

  std::pair<blk_hash_t, bool> softVotedBlockForRound_(std::vector<Vote> &votes,
                                                      uint64_t round);

  std::pair<blk_hash_t, bool> proposeMyPbftBlock_();

  std::pair<blk_hash_t, bool> identifyLeaderBlock_();

  bool pushPbftBlockIntoChain_(uint64_t round,
                               blk_hash_t const &cert_voted_block_hash);

  bool updatePbftChainDB_(PbftBlock const &pbft_block);

  bool checkPbftBlockValid_(blk_hash_t const &block_hash) const;

  void syncPbftChainFromPeers_();

  bool comparePbftCSblockWithDAGblocks_(blk_hash_t const &cs_block_hash);

  bool stopped_ = true;
  PbftBlockTypes next_pbft_block_type_ = pbft_block_none_type;
  // Using to check if PBFT CS block has proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  std::weak_ptr<FullNode> node_;
  std::shared_ptr<std::thread> daemon_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<TaraxaCapability> capability_;
  // Database
  std::shared_ptr<SimpleDBFace> db_votes_;
  std::shared_ptr<SimpleDBFace> db_pbftchain_;

  uint64_t pbft_round_ = 1;
  size_t pbft_step_ = 1;

  size_t sortition_threshold_;
  size_t TWO_T_PLUS_ONE;  // This is 2t+1

  std::string genesis_;

  // TODO: will remove later, TEST CODE
  void countVotes_();
  std::shared_ptr<std::thread> monitor_votes_;
  bool monitor_stop_ = true;
  size_t last_step_ = 0;
  std::chrono::system_clock::time_point last_step_clock_initial_datetime_;
  std::chrono::system_clock::time_point current_step_clock_initial_datetime_;
  // END TEST CODE

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

  mutable dev::Logger log_inf_test_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_TEST")};
};

}  // namespace taraxa

#endif  // PBFT_MANAGER_H
