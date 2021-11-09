#pragma once

#include <string>
#include <thread>

#include "common/types.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft/sync_queue.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 13
#define MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS 20
#define MAX_WAIT_FOR_NEXT_VOTED_BLOCK_STEPS 20

namespace taraxa {
class FullNode;

enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

enum PbftSyncRequestReason {
  missing_dag_blk = 1,
  invalid_cert_voted_block,
  invalid_soft_voted_block,
  exceeded_max_steps
};

class PbftManager : public std::enable_shared_from_this<PbftManager> {
 public:
  using time_point = std::chrono::system_clock::time_point;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;

  PbftManager(PbftConfig const &conf, blk_hash_t const &genesis, addr_t node_addr, std::shared_ptr<DbStorage> db,
              std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
              std::shared_ptr<NextVotesManager> next_votes_mgr, std::shared_ptr<DagManager> dag_mgr,
              std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<TransactionManager> trx_mgr,
              std::shared_ptr<final_chain::FinalChain> final_chain, secret_t node_sk, vrf_sk_t vrf_sk);
  ~PbftManager();

  void setNetwork(std::weak_ptr<Network> network);
  void start();
  void stop();
  void run();

  uint64_t getVoteWeight(PbftVoteTypes type, uint64_t round, size_t step) const;

  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);
  uint64_t getPbftRound() const;
  uint64_t getPbftStep() const;
  void setPbftRound(uint64_t const round);
  size_t getSortitionThreshold() const;
  size_t getPreviousPbftPeriodSortitionThreshold() const;
  size_t getTwoTPlusOne() const;
  void setPbftStep(size_t const pbft_step);

  std::shared_ptr<Vote> generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t round, size_t step);

  size_t getDposTotalVotesCount() const;
  size_t getPreviousPbftPeriodDposTotalVotesCount() const;
  size_t getDposWeightedVotesCount() const;

  uint64_t pbftSyncingPeriod() const;
  size_t syncBlockQueueSize() const;
  void syncBlockQueuePush(SyncBlock &&block, dev::p2p::NodeID const &node_id);

  // Notice: Test purpose
  // TODO: Add a check for some kind of guards to ensure these are only called from within a test
  void setSortitionThreshold(size_t const sortition_threshold);
  std::vector<std::vector<uint>> createMockTrxSchedule(
      std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>> trx_overlap_table);
  size_t getPbftCommitteeSize() const { return COMMITTEE_SIZE; }
  u_long getPbftInitialLambda() const { return LAMBDA_ms_MIN; }
  void setLastSoftVotedValue(blk_hash_t soft_voted_value);
  void resume();
  void resumeSingleState();
  void setMaxWaitForSoftVotedBlock_ms(uint64_t wait_ms);
  void setMaxWaitForNextVotedBlock_ms(uint64_t wait_ms);

 private:
  // DPOS
  void updateDposState_();
  size_t dposEligibleVoteCount_(addr_t const &addr);

  void resetStep_();
  bool resetRound_();
  void sleep_();

  void initialState_();
  void continuousOperation_();
  // This one is for tests only...
  void doNextState_();

  void setNextState_();
  void setFilterState_();
  void setCertifyState_();
  void setFinishState_();
  void setFinishPollingState_();
  void loopBackFinishState_();

  bool stateOperations_();
  void proposeBlock_();
  void identifyBlock_();
  void certifyBlock_();
  void firstFinish_();
  void secondFinish_();

  size_t placeVote_(blk_hash_t const &blockhash, PbftVoteTypes vote_type, uint64_t round, size_t step);

  std::pair<blk_hash_t, bool> proposeMyPbftBlock_();

  std::pair<blk_hash_t, bool> identifyLeaderBlock_();

  bool syncRequestedAlreadyThisStep_() const;

  void syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash);

  bool broadcastAlreadyThisStep_() const;

  bool comparePbftBlockScheduleWithDAGblocks_(blk_hash_t const &pbft_block_hash);
  bool comparePbftBlockScheduleWithDAGblocks_(const std::shared_ptr<PbftBlock> &pbft_block);

  bool pushCertVotedPbftBlockIntoChain_(blk_hash_t const &cert_voted_block_hash,
                                        std::vector<std::shared_ptr<Vote>> &&cert_votes_for_round);

  void pushSyncedPbftBlocksIntoChain_();

  void finalize_(SyncBlock &&sync_block, bool synchronous_processing = false);
  bool pushPbftBlock_(SyncBlock &&sync_block);

  void updateTwoTPlusOneAndThreshold_();
  bool is_syncing_();

  bool giveUpNextVotedBlock_();
  bool giveUpSoftVotedBlock_();
  void initializeVotedValueTimeouts_();
  void updateLastSoftVotedValue_(blk_hash_t const new_soft_voted_value);
  void checkPreviousRoundNextVotedValueChange_();
  bool updateSoftVotedBlockForThisRound_();

  blk_hash_t calculateOrderHash(std::vector<blk_hash_t> const &dag_block_hashes,
                                std::vector<trx_hash_t> const &trx_hashes);
  blk_hash_t calculateOrderHash(std::vector<DagBlock> const &dag_blocks, std::vector<Transaction> const &transactions);

  std::optional<SyncBlock> processSyncBlock();

  std::shared_ptr<PbftBlock> getUnfinalizedBlock_(blk_hash_t const &block_hash);

  std::atomic<bool> stopped_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);

  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<NextVotesManager> next_votes_manager_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;

  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;

  u_long const LAMBDA_ms_MIN;
  u_long LAMBDA_ms = 0;
  u_long LAMBDA_backoff_multiple = 1;

  std::default_random_engine random_engine_{std::random_device{}()};

  size_t const COMMITTEE_SIZE;
  size_t const DAG_BLOCKS_SIZE;
  size_t const GHOST_PATH_MOVE_BACK;
  bool RUN_COUNT_VOTES;  // TODO: Only for test, need remove later

  PbftStates state_ = value_proposal_state;
  std::atomic<uint64_t> round_ = 1;
  size_t step_ = 1;
  size_t startingStepInRound_ = 1;

  blk_hash_t own_starting_value_for_round_ = NULL_BLOCK_HASH;

  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);

  // Full sync block for pbft block that is being currently cert voted for
  SyncBlock cert_sync_block_;

  time_point round_clock_initial_datetime_;
  time_point now_;

  time_point time_began_waiting_next_voted_block_;
  time_point time_began_waiting_soft_voted_block_;
  blk_hash_t previous_round_next_voted_value_ = NULL_BLOCK_HASH;
  bool previous_round_next_voted_null_block_hash_ = false;

  blk_hash_t last_soft_voted_value_ = NULL_BLOCK_HASH;
  blk_hash_t last_cert_voted_value_ = NULL_BLOCK_HASH;

  std::chrono::duration<double> duration_;
  u_long next_step_time_ms_ = 0;
  u_long elapsed_time_in_round_ms_ = 0;

  bool executed_pbft_block_ = false;
  bool have_executed_this_round_ = false;
  bool should_have_cert_voted_in_this_round_ = false;
  bool next_voted_soft_value_ = false;
  bool next_voted_null_block_hash_ = false;
  bool go_finish_state_ = false;
  bool loop_back_finish_state_ = false;
  bool polling_state_print_log_ = true;

  uint64_t max_wait_for_soft_voted_block_steps_ms_ = 30;
  uint64_t max_wait_for_next_voted_block_steps_ms_ = 30;

  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;
  uint64_t pbft_round_last_broadcast_ = 0;
  size_t pbft_step_last_broadcast_ = 0;

  std::atomic<uint64_t> dpos_period_ = 0;
  std::atomic<size_t> dpos_votes_count_ = 0;
  std::atomic<size_t> previous_pbft_period_dpos_votes_count_ = 0;
  std::atomic<size_t> weighted_votes_count_ = 0;

  size_t sortition_threshold_ = 0;
  std::atomic<size_t> previous_pbft_period_sortition_threshold_ = 0;
  // 2t+1 minimum number of votes for consensus
  size_t TWO_T_PLUS_ONE = 0;

  blk_hash_t dag_genesis_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  SyncBlockQueue sync_queue_;

  // TODO: will remove later, TEST CODE
  void countVotes_();

  std::shared_ptr<std::thread> monitor_votes_ = nullptr;
  std::atomic<bool> monitor_stop_ = true;
  size_t last_step_ = 0;
  time_point last_step_clock_initial_datetime_;
  time_point current_step_clock_initial_datetime_;
  // END TEST CODE

  LOG_OBJECTS_DEFINE
  mutable logger::Logger log_nf_test_{logger::createLogger(taraxa::logger::Verbosity::Info, "PBFT_TEST", node_addr_)};
};

}  // namespace taraxa
