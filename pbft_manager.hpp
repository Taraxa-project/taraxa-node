#ifndef TARAXA_NODE_PBFT_MANAGER_HPP
#define TARAXA_NODE_PBFT_MANAGER_HPP

#include <libdevcore/Log.h>

#include <atomic>
#include <string>
#include <thread>

#include "config.hpp"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.hpp"
#include "replay_protection_service.hpp"
#include "taraxa_capability.hpp"
#include "types.hpp"
#include "vote.hpp"

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS_DECIMAL 9007199254740991
#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 50

namespace taraxa {
class FullNode;

enum PbftStates {
  value_proposal_state = 1,
  filter_state,
  certify_state,
  finish_state,
  finish_polling_state
};

class PbftManager {
  unique_ptr<ReplayProtectionService> replay_protection_service;

 public:
  using time_point = std::chrono::system_clock::time_point;

  explicit PbftManager(std::string const &genesis);
  PbftManager(PbftConfig const &conf, std::string const &genesis);
  ~PbftManager();

  void setFullNode(std::shared_ptr<FullNode> node);
  void start();
  void stop();
  void run();

  blk_hash_t getLastPbftBlockHashAtStartOfRound() const;
  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);
  std::string getScheduleBlockByPeriod(uint64_t const period);

  size_t getSortitionThreshold() const;
  void setSortitionThreshold(size_t const sortition_threshold);
  size_t getTwoTPlusOne() const;
  void setTwoTPlusOne(size_t const two_t_plus_one);
  void setPbftStep(size_t const pbft_step);

  // Notice: Test purpose
  void setPbftThreshold(size_t const threshold);
  void setPbftRound(uint64_t const pbft_round);
  uint64_t getPbftRound() const;
  size_t getPbftStep() const;
  // End Test

  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step);

  // <account address, PbftSortitionAccount>
  // Temporary table for executor to update
  std::unordered_map<addr_t, PbftSortitionAccount>
      sortition_account_balance_table_tmp;
  // Permanent table update at beginning each of PBFT new round
  std::unordered_map<addr_t, PbftSortitionAccount>
      sortition_account_balance_table;

  u_long LAMBDA_ms_MIN;
  u_long LAMBDA_ms;
  size_t COMMITTEE_SIZE = 0;           // TODO: Only for test, need remove later
  uint64_t VALID_SORTITION_COINS = 0;  // TODO: Only for test, need remove later
  size_t DAG_BLOCKS_SIZE = 0;          // TODO: Only for test, need remove later
  size_t GHOST_PATH_MOVE_BACK = 0;     // TODO: Only for test, need remove later
  // When PBFT pivot block finalized, period = period + 1,
  // but last_seen = period. SKIP_PERIODS = 1 means not skip any periods.
  uint64_t SKIP_PERIODS = 0;
  bool RUN_COUNT_VOTES = 0;  // TODO: Only for test, need remove later

 private:
  void resetStep_();
  bool resetRound_();
  void sleep_();

  void initialState_();
  void setNextState_();
  void setFilterState_();
  void setCertifyState_();
  void setFinishState_();
  void setFinishPollingState_();
  void continueFinishPollingState_(size_t step);
  void loopBackFinishState_();

  bool stateOperations_();
  void proposeBlock_();
  void identifyBlock_();
  void certifyBlock_();
  void firstFinish_();
  void secondFinish_();

  uint64_t roundDeterminedFromVotes_();

  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(
      std::vector<Vote> const &votes) const;

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

  std::pair<blk_hash_t, bool> identifyLeaderBlock_(
      std::vector<Vote> const &votes);

  bool checkPbftBlockValid_(blk_hash_t const &block_hash) const;

  bool syncRequestedAlreadyThisStep_() const;

  void syncPbftChainFromPeers_();

  bool comparePbftBlockScheduleWithDAGblocks_(
      blk_hash_t const &pbft_block_hash);
  bool comparePbftBlockScheduleWithDAGblocks_(PbftBlock const &pbft_block);

  bool pushCertVotedPbftBlockIntoChain_(
      blk_hash_t const &cert_voted_block_hash,
      std::vector<Vote> const &cert_votes_for_round);

  void pushSyncedPbftBlocksIntoChain_();

  bool pushPbftBlock_(PbftBlock const &pbft_block,
                      std::vector<Vote> const &cert_votes);

  void updateTwoTPlusOneAndThreshold_();

  void updateTempSortitionAccountsTable_(
      uint64_t period, unordered_set<addr_t> const &dag_block_proposers,
      unordered_set<addr_t> const &trx_senders,
      unordered_map<addr_t, val_t> const &execution_touched_account_balances);
  void updateSortitionAccountsTable_();

  void updateSortitionAccountsDB_(DbStorage::BatchPtr const &batch);

  std::atomic<bool> stopped_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  std::weak_ptr<FullNode> node_;
  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<TaraxaCapability> capability_;

  size_t valid_sortition_accounts_size_ = 0;
  // Database
  std::shared_ptr<DbStorage> db_ = nullptr;

  blk_hash_t pbft_chain_last_block_hash_;
  std::pair<blk_hash_t, bool> next_voted_block_from_previous_round_;

  PbftStates state_;
  uint64_t round_;
  size_t step_;
  u_long STEP_4_DELAY;  // constant

  blk_hash_t own_starting_value_for_round_;
  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round_;
  // <round, block_hash_added_into_chain>
  std::unordered_map<size_t, blk_hash_t> push_block_values_for_round_;
  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_;
  std::vector<Vote> votes_;

  time_point round_clock_initial_datetime_;
  time_point now_;
  std::chrono::duration<double> duration_;
  long next_step_time_ms_;
  long elapsed_time_in_round_ms_;

  bool executed_pbft_block_;
  bool have_executed_this_round_;
  bool should_have_cert_voted_in_this_round_;
  bool next_voted_soft_value_;
  bool next_voted_null_block_hash_;
  bool continue_finish_polling_state_;
  bool go_finish_state_;
  bool loop_back_finish_state_;

  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;

  size_t pbft_last_observed_synced_queue_size_ = 0;

  uint64_t last_period_should_speak_ = 0;

  size_t sortition_threshold_ = 0;
  size_t TWO_T_PLUS_ONE = 0;  // This is 2t+1
  bool is_active_player_ = false;

  std::string dag_genesis_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  // TODO: will remove later, TEST CODE
  void countVotes_();

  std::shared_ptr<std::thread> monitor_votes_;
  std::atomic<bool> monitor_stop_ = true;
  size_t last_step_;
  time_point last_step_clock_initial_datetime_;
  time_point current_step_clock_initial_datetime_;
  // END TEST CODE

  std::atomic<uint64_t> num_executed_trx_ = 0;
  std::atomic<uint64_t> num_executed_blk_ = 0;
  unordered_set<addr_t> dag_block_proposers_tmp_;
  dev::eth::Transactions transactions_tmp_;
  unordered_set<addr_t> trx_senders_tmp_;

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
