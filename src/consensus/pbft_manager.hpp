#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "common/types.hpp"
#include "config/config.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "network/taraxa_capability.hpp"
#include "node/executor.hpp"
#include "pbft_chain.hpp"
#include "vote.hpp"
#include "vrf_wrapper.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 13

namespace taraxa {
class FullNode;

enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

enum PbftSyncRequestReason {
  missing_dag_blk = 1,
  invalid_cert_voted_block,
  invalid_soft_voted_block,
  exceeded_max_steps
};

class PbftManager {
 public:
  using time_point = std::chrono::system_clock::time_point;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;

  PbftManager(PbftConfig const &conf, std::string const &genesis, addr_t node_addr, std::shared_ptr<DbStorage> db,
              std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
              std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr, std::shared_ptr<DagManager> dag_mgr,
              std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<Executor> executor, secret_t node_sk, vrf_sk_t vrf_sk);
  ~PbftManager();

  void setNetwork(std::weak_ptr<Network> network);
  void start();
  void stop();
  void run();

  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step, size_t weighted_index);

  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);
  uint64_t getPbftRound() const;
  void setPbftRound(uint64_t const round);
  size_t getSortitionThreshold() const;
  size_t getTwoTPlusOne() const;
  void setTwoTPlusOne(size_t const two_t_plus_one);
  void setPbftStep(size_t const pbft_step);

  Vote generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t round, size_t step, size_t weighted_index,
                    blk_hash_t const &last_pbft_block_hash);
  size_t getDposTotalVotesCount() const;
  size_t getDposWeightedVotesCount() const;

  // Notice: Test purpose
  void setSortitionThreshold(size_t const sortition_threshold);
  std::vector<std::vector<uint>> createMockTrxSchedule(
      std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>> trx_overlap_table);
  size_t getPbftCommitteeSize() const { return COMMITTEE_SIZE; }
  u_long getPbftInitialLambda() const { return LAMBDA_ms_MIN; }

 private:
  // DPOS
  void update_dpos_state_();
  size_t dpos_eligible_vote_count_(addr_t const &addr);

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

  std::pair<blk_hash_t, bool> blockWithEnoughVotes_(std::vector<Vote> const &votes) const;

  std::vector<Vote> getVotesOfTypeFromVotesForRoundAndStep_(PbftVoteTypes vote_type, std::vector<Vote> &votes,
                                                            uint64_t round, size_t step,
                                                            blk_hash_t const &last_pbft_block_hash,
                                                            std::pair<blk_hash_t, bool> blockhash);

  size_t placeVote_(blk_hash_t const &blockhash, PbftVoteTypes vote_type, uint64_t round, size_t step);

  std::pair<blk_hash_t, bool> proposeMyPbftBlock_();

  std::pair<blk_hash_t, bool> identifyLeaderBlock_(std::vector<Vote> const &votes);

  bool checkPbftBlockValid_(blk_hash_t const &block_hash) const;

  bool syncRequestedAlreadyThisStep_() const;

  void syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash);

  bool broadcastAlreadyThisStep_() const;

  bool comparePbftBlockScheduleWithDAGblocks_(blk_hash_t const &pbft_block_hash);
  bool comparePbftBlockScheduleWithDAGblocks_(PbftBlock const &pbft_block);

  bool pushCertVotedPbftBlockIntoChain_(blk_hash_t const &cert_voted_block_hash,
                                        std::vector<Vote> const &cert_votes_for_round);

  void pushSyncedPbftBlocksIntoChain_();

  bool pushPbftBlock_(PbftBlockCert const &pbft_block_cert_votes, bool syncing, bool &updated_vrf_last_pbft_block_hash);

  void updateTwoTPlusOneAndThreshold_();
  bool is_syncing_();

  std::atomic<bool> stopped_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);

  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<NextVotesForPreviousRound> previous_round_next_votes_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<Executor> executor_;

  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;

  u_long const LAMBDA_ms_MIN;
  u_long LAMBDA_ms = 0;
  size_t const COMMITTEE_SIZE;
  size_t DAG_BLOCKS_SIZE;
  size_t GHOST_PATH_MOVE_BACK;
  bool RUN_COUNT_VOTES;  // TODO: Only for test, need remove later

  PbftStates state_ = value_proposal_state;
  std::atomic<uint64_t> round_ = 1;
  size_t step_ = 1;
  u_long STEP_4_DELAY = 0;  // constant

  blk_hash_t vrf_pbft_chain_last_block_hash_ = blk_hash_t(0);

  blk_hash_t own_starting_value_for_round_ = NULL_BLOCK_HASH;
  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round_;
  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);

  std::vector<Vote> votes_;

  time_point round_clock_initial_datetime_;
  time_point now_;
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

  uint64_t round_began_wait_proposal_block_ = 0;
  size_t max_wait_rounds_for_proposal_block_ = 5;

  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;
  uint64_t pbft_round_last_broadcast_ = 0;
  size_t pbft_step_last_broadcast_ = 0;

  size_t pbft_last_observed_synced_queue_size_ = 0;

  std::atomic<uint64_t> dpos_period_;
  std::atomic<size_t> dpos_votes_count_;
  std::atomic<size_t> weighted_votes_count_;

  size_t sortition_threshold_ = 0;
  // 2t+1 minimum number of votes for consensus
  size_t TWO_T_PLUS_ONE = 0;

  std::string dag_genesis_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

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
