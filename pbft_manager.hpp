#ifndef TARAXA_NODE_PBFT_MANAGER_HPP
#define TARAXA_NODE_PBFT_MANAGER_HPP

#include <string>
#include <thread>

#include <libdevcore/Log.h>
#include <atomic>
#include "config.hpp"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.hpp"
#include "taraxa_capability.hpp"
#include "types.hpp"
#include "vote.hpp"

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS_DECIMAL 9007199254740991
#define NULL_BLOCK_HASH blk_hash_t(0)
#define LAMBDA_ms 1000           // milliseconds
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 50
#define COMMITTEE_SIZE 3  // TODO: The value for local test, need to change
#define VALID_SORTITION_COINS 10000  // TODO: the value may change later
#undef LAMBDA_ms                     // TODO: undef for test, need remove later
#undef COMMITTEE_SIZE                // TODO: undef for test, need remove later
#undef VALID_SORTITION_COINS  // // TODO: undef for test, need remove later

namespace taraxa {
class FullNode;

class PbftManager {
 public:
  using ReplayProtectionService = replay_protection::ReplayProtectionService;

  PbftManager(std::string const &genesis);
  PbftManager(std::vector<uint> const &params, std::string const &genesis);
  ~PbftManager() { stop(); }

  void setFullNode(
      std::shared_ptr<FullNode> node,
      std::shared_ptr<ReplayProtectionService> replay_protection_service);
  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step);
  void start();
  void stop();
  void run();

  blk_hash_t getLastPbftBlockHashAtStartOfRound() const {
    return pbft_chain_last_block_hash_;
  }

  std::string getScheduleBlockByPeriod(uint64_t period);

  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);

  size_t getSortitionThreshold() const { return sortition_threshold_; }
  void setSortitionThreshold(size_t const sortition_threshold) {
    sortition_threshold_ = sortition_threshold;
  }
  void setTwoTPlusOne(size_t const two_t_plus_one) {
    TWO_T_PLUS_ONE = two_t_plus_one;
  }
  size_t getTwoTPlusOne() const { return TWO_T_PLUS_ONE; }

  // TODO: only for test
  void setPbftThreshold(size_t const threshold) {
    sortition_threshold_ = threshold;
  }
  void setPbftRound(uint64_t const pbft_round) { pbft_round_ = pbft_round; }
  void setPbftStep(size_t const pbft_step) { pbft_step_ = pbft_step; }
  uint64_t getPbftRound() const { return pbft_round_; }
  size_t getPbftStep() const { return pbft_step_; }

  // TODO: Maybe don't need account balance in the table
  // <account address, PbftSortitionAccount>
  // Temp table for executor to update
  std::unordered_map<addr_t, PbftSortitionAccount>
      sortition_account_balance_table;

  u_long LAMBDA_ms;                // TODO: Only for test, need remove later
  size_t COMMITTEE_SIZE;           // TODO: Only for test, need remove later
  uint64_t VALID_SORTITION_COINS;  // TODO: Only for test, need remove later
  size_t DAG_BLOCKS_SIZE;          // TODO: Only for test, need remove later
  size_t GHOST_PATH_MOVE_BACK;     // TODO: Only for test, need remove later
  bool RUN_COUNT_VOTES;            // TODO: Only for test, need remove later
  // When PBFT pivot block finalized, period = period + 1,
  // but last_seen = period. SKIP_PERIODS = 1 means not skip any periods.
  uint64_t SKIP_PERIODS = 1;

 private:
  uint64_t roundDeterminedFromVotes_();

  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(
      std::vector<Vote> &votes) const;

  bool nullBlockNextVotedForRoundAndStep_(std::vector<Vote> &votes,
                                          uint64_t round);

  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
  getVotesOfTypeFromVotesForRoundByStep_(PbftVoteTypes vote_type,
                                         std::vector<Vote> &votes,
                                         uint64_t round,
                                         std::pair<blk_hash_t, bool> blockhash);
  std::vector<Vote> getVotesOfTypeFromVotesForRoundAndStep_(
      PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
      size_t step, std::pair<blk_hash_t, bool> blockhash);

  std::pair<blk_hash_t, bool> nextVotedBlockForRoundAndStep_(
      std::vector<Vote> &votes, uint64_t round);

  void placeVote_(blk_hash_t const &blockhash, PbftVoteTypes vote_type,
                  uint64_t round, size_t step);

  std::pair<blk_hash_t, bool> softVotedBlockForRound_(std::vector<Vote> &votes,
                                                      uint64_t round);

  std::pair<blk_hash_t, bool> proposeMyPbftBlock_();

  std::pair<blk_hash_t, bool> identifyLeaderBlock_(std::vector<Vote> &votes);

  bool pushCertVotedPbftBlockIntoChain_(
      blk_hash_t const &cert_voted_block_hash);

  bool updatePbftChainDB_(PbftBlock const &pbft_block);

  bool checkPbftBlockInUnverifiedQueue_(blk_hash_t const &block_hash) const;

  bool checkPbftBlockValid_(blk_hash_t const &block_hash) const;

  void syncPbftChainFromPeers_();

  bool comparePbftBlockScheduleWithDAGblocks_(
      blk_hash_t const &pbft_block_hash);
  bool comparePbftBlockScheduleWithDAGblocks_(PbftBlock const &pbft_block);

  void pushSyncedPbftBlocksIntoChain_();

  bool pushPbftBlockIntoChain_(PbftBlock const &pbft_block);

  void updateTwoTPlusOneAndThreshold_();

  void updateSortitionAccountsDB_();

  size_t getValidPbftSortitionPlayerSize_();

  std::atomic<bool> stopped_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  std::weak_ptr<FullNode> node_;
  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<TaraxaCapability> capability_;
  std::shared_ptr<ReplayProtectionService> replay_protection_service_;

  size_t valid_sortition_accounts_size_;
  // Database
  std::shared_ptr<DbStorage> db_ = nullptr;

  blk_hash_t pbft_chain_last_block_hash_;

  uint64_t pbft_round_;
  uint64_t pbft_round_last_;
  size_t pbft_step_;
  bool executed_pbft_block_ = false;

  uint64_t pbft_round_last_requested_sync_;
  size_t pbft_step_last_requested_sync_;

  size_t pbft_last_observed_synced_queue_size_ = 0;

  uint64_t last_period_should_speak_ = 0;

  uint64_t last_pbft_syncing_height_;

  size_t sortition_threshold_;
  size_t TWO_T_PLUS_ONE;  // This is 2t+1

  bool is_active_player_ = false;

  std::string dag_genesis_;

  // TODO: will remove later, TEST CODE
  void countVotes_();

  std::shared_ptr<std::thread> monitor_votes_;
  std::atomic<bool> monitor_stop_ = true;
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
