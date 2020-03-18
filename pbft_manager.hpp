#ifndef TARAXA_NODE_PBFT_MANAGER_HPP
#define TARAXA_NODE_PBFT_MANAGER_HPP

#include <libdevcore/Log.h>

#include <atomic>
#include <string>
#include <thread>

#include "config.hpp"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.hpp"
#include "pbft_state_machine.hpp"
#include "taraxa_capability.hpp"
#include "types.hpp"
#include "vote.hpp"

#define MAX_STEPS 50
// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS_DECIMAL 9007199254740991
#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define COMMITTEE_SIZE 3  // TODO: The value for local test, need to change
#define VALID_SORTITION_COINS 10000  // TODO: the value may change later
#undef COMMITTEE_SIZE                // TODO: undef for test, need remove later
#undef VALID_SORTITION_COINS         // TODO: undef for test, need remove later

namespace taraxa {
class FullNode;

enum PbftStates {
  // Step 0
  reset_state,
  // Step 1
  roll_call_state,
  propose_block_from_self_state,
  propose_block_from_prev_round_state,
  // Step 2
  soft_vote_block_from_leader,
  soft_vote_block_from_prev_round_state,
  // Step 3
  cert_vote_block_polling_state,
  // Step 4
  chain_block_state,
  // Step 5...
  vote_next_block_state,
  vote_next_block_polling_state,
  num_pbft_states
};

class PbftManager {
  friend class PbftStateMachine;

 public:
  using ReplayProtectionService = replay_protection::ReplayProtectionService;

  explicit PbftManager(std::string const &genesis);
  PbftManager(std::vector<uint> const &params, std::string const &genesis);
  ~PbftManager() { stop(); }

  void setFullNode(
      std::shared_ptr<FullNode> node,
      std::shared_ptr<ReplayProtectionService> replay_protection_service);
  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step);
  void start();
  void stop();

  uint64_t getRound() const { return round_; }
  uint64_t getLastRound() const { return last_round_; }
  uint64_t getStep() const { return step_; }
  uint64_t getLastStep() const { return last_step_; }
  int getState() const { return state_; }
  int getLastState() const { return last_state_; }

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

  // TODO: Maybe don't need account balance in the table
  // <account address, PbftSortitionAccount>
  // Temporary table for executor to update
  std::unordered_map<addr_t, PbftSortitionAccount>
      sortition_account_balance_table_tmp;
  // Permanent table update at beginning each of PBFT new round
  std::unordered_map<addr_t, PbftSortitionAccount>
      sortition_account_balance_table;

  u_long LAMBDA_ms_MIN = 0;
  size_t COMMITTEE_SIZE = 0;           // TODO: Only for test, need remove later
  uint64_t VALID_SORTITION_COINS = 0;  // TODO: Only for test, need remove later
  size_t DAG_BLOCKS_SIZE = 0;          // TODO: Only for test, need remove later
  size_t GHOST_PATH_MOVE_BACK = 0;     // TODO: Only for test, need remove later
  // When PBFT pivot block finalized, period = period + 1,
  // but last_seen = period. SKIP_PERIODS = 1 means not skip any periods.
  uint64_t SKIP_PERIODS = 0;
  bool RUN_COUNT_VOTES = 0;  // TODO: Only for test, need remove later

 private:
  uint64_t roundDeterminedFromVotes_(std::vector<Vote> votes);

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

  std::pair<blk_hash_t, bool> proposeOwnBlock_();

  std::pair<blk_hash_t, bool> identifyLeaderBlock_(
      std::vector<Vote> const &votes);

  bool updatePbftChainDB_(PbftBlock const &pbft_block);

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

  void updateSortitionAccountsTable_();

  void updateSortitionAccountsDB_(DbStorage::BatchPtr const &batch);

  std::atomic<bool> stop_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  std::shared_ptr<PbftStateMachine> pbft_state_machine_;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<TaraxaCapability> capability_;
  std::shared_ptr<ReplayProtectionService> replay_protection_service_;

  size_t valid_sortition_accounts_size_ = 0;
  // Database
  std::shared_ptr<DbStorage> db_ = nullptr;

  bool sync_peers_pbft_chain_;

  blk_hash_t pbft_chain_last_block_hash_;

<<<<<<< HEAD
  uint64_t pbft_round_ = 0;
  uint64_t pbft_round_last_ = 0;
  size_t pbft_step_ = 0;
  bool executed_pbft_block_ = false;

  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;
=======
  bool have_chained_block_;

  uint64_t last_sync_round_;
  uint64_t last_sync_step_;
>>>>>>> Added PBFT state machine

  size_t last_chain_synced_queue_size_ = 0;

  uint64_t last_chain_period_ = 0;

  size_t sortition_threshold_ = 0;
  size_t TWO_T_PLUS_ONE = 0;  // This is 2t+1
  bool is_active_player_ = false;

  std::string dag_genesis_;

  // TODO: will remove later, TEST CODE
  void countVotes_();

  std::shared_ptr<std::thread> monitor_votes_;
  std::atomic<bool> stop_monitor_ = true;
  // END TEST CODE

  // State machine data
  uint64_t round_;
  uint64_t last_round_;
  uint64_t step_;
  uint64_t last_step_;
  std::chrono::system_clock::time_point round_start_clock_time_;
  std::chrono::system_clock::time_point last_round_start_clock_time_;
  std::vector<Vote> round_votes_;

  int state_;
  int last_state_;
  std::chrono::system_clock::time_point state_start_clock_time_;
  std::chrono::system_clock::time_point last_state_start_clock_time_;
  u_long state_lambda_ms_;
  u_long state_max_time_ms_;  // Maximum time for current state

  blk_hash_t own_starting_value_for_round_;

  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_;
  std::pair<blk_hash_t, bool> next_voted_block_from_previous_round_;

  bool have_next_voted_soft_value_;
  bool have_next_voted_null_block_hash_;
  bool have_cert_voted_this_round_;

  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round_;
  // <round, block_hash_added_into_chain>
  std::unordered_map<size_t, blk_hash_t> chained_blocks_for_round_;

  // End state machine data

  // State machine methods

  uint64_t getNumStepsOverMax()
      const;  // Get the number of steps executed, beyond the step limit
  u_long getRoundElapsedTimeMs()
      const;  // Get the time(ms), since the current round started
  u_long getStateElapsedTimeMs()
      const;  // Get the time(ms), since the current state started
  u_long getLastStateElapsedTimeMs()
      const;  // Get the time(ms), since the last state started
  u_long getNextStateCheckTimeMs()
      const;  // Get the clock time(ms) to sleep, until the next state check
  bool hasNextVotedBlockFromLastRound() const;
  bool hasChainedBlockFromRound() const;

  void resetRound();              // Initialize rounds
  void setRound(uint64_t round);  // Set new round

  void resetState();         // Initialize state for new round
  void setState(int state);  // Set new state, in round

  void syncChain();  // Sync chain state

  void executeState();  // Execute the current state

  static bool isPollingState(int state);

  // End state machine methods

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
