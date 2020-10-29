#ifndef TARAXA_NODE_PBFT_MANAGER_HPP
#define TARAXA_NODE_PBFT_MANAGER_HPP

#include <atomic>
#include <string>
#include <thread>

#include "block_proposer.hpp"
#include "config.hpp"
#include "net/WSServer.h"
#include "pbft_chain.hpp"
#include "replay_protection_service.hpp"
#include "taraxa_capability.hpp"
#include "transaction_order_manager.hpp"
#include "types.hpp"
#include "vote.hpp"

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 13

namespace taraxa {
class FullNode;
class WSServer;

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

  PbftManager(PbftConfig const &conf, std::string const &genesis,
              addr_t node_addr, std::shared_ptr<DbStorage> db,
              std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<VoteManager> vote_mgr,
              std::shared_ptr<DagManager> dag_mgr,
              std::shared_ptr<BlockManager> blk_mgr,
              std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<TransactionOrderManager> trx_ord_mgr,
              std::shared_ptr<TransactionManager> trx_mgr, secret_t node_sk,
              vrf_sk_t vrf_sk, uint32_t expected_max_trx_per_block);
  ~PbftManager();

  void setNetwork(std::shared_ptr<Network> network);
  void setWSServer(std::shared_ptr<net::WSServer> ws_server);
  void start();
  void stop();
  void run();

  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);

  uint64_t getPbftRound() const;
  void setPbftRound(uint64_t const round);
  size_t getSortitionThreshold() const;
  size_t getTwoTPlusOne() const;
  void setTwoTPlusOne(size_t const two_t_plus_one);
  void setPbftStep(size_t const pbft_step);
  void getNextVotesForLastRound(std::vector<Vote> &next_votes_bundle);
  void updateNextVotesForRound(std::vector<Vote> next_votes);

  Vote generateVote(blk_hash_t const &blockhash, PbftVoteTypes type,
                    uint64_t period, size_t step,
                    blk_hash_t const &last_pbft_block_hash);

  // Notice: Test purpose
  void setSortitionThreshold(size_t const sortition_threshold);
  std::vector<std::vector<uint>> createMockTrxSchedule(
      std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
          trx_overlap_table);
  bool shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step);

  u_long const LAMBDA_ms_MIN;

 private:
  u_long LAMBDA_ms = 0;

 public:
  size_t const COMMITTEE_SIZE;

 private:
  size_t DAG_BLOCKS_SIZE;
  size_t GHOST_PATH_MOVE_BACK;
  bool RUN_COUNT_VOTES;  // TODO: Only for test, need remove later

  void update_dpos_state_();
  bool is_eligible_(addr_t const &addr);

 public:
  uint64_t getEligibleVoterCount() const;

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

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

  bool nextVotesSyncAlreadyThisRoundStep_();

  void syncNextVotes_();

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

  std::atomic<bool> stopped_ = true;
  // Using to check if PBFT block has been proposed already in one period
  std::pair<blk_hash_t, bool> proposed_block_hash_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  std::unique_ptr<std::thread> daemon_ = nullptr;
  std::shared_ptr<VoteManager> vote_mgr_ = nullptr;
  std::shared_ptr<PbftChain> pbft_chain_ = nullptr;
  std::shared_ptr<DagManager> dag_mgr_ = nullptr;
  std::shared_ptr<Network> network_ = nullptr;
  std::shared_ptr<TaraxaCapability> capability_ = nullptr;
  std::shared_ptr<BlockManager> blk_mgr_;
  std::shared_ptr<net::WSServer> ws_server_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<TransactionOrderManager> trx_ord_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;

  // Database
  std::shared_ptr<DbStorage> db_ = nullptr;

  blk_hash_t pbft_chain_last_block_hash_ = blk_hash_t(0);
  std::pair<blk_hash_t, bool> next_voted_block_from_previous_round_ =
      std::make_pair(NULL_BLOCK_HASH, false);

  PbftStates state_ = value_proposal_state;
  uint64_t round_ = 1;
  size_t step_ = 1;
  u_long STEP_4_DELAY = 0;  // constant

  blk_hash_t own_starting_value_for_round_ = NULL_BLOCK_HASH;
  // <round, cert_voted_block_hash>
  std::unordered_map<size_t, blk_hash_t> cert_voted_values_for_round_;
  // <round, block_hash_added_into_chain>
  std::unordered_map<size_t, blk_hash_t> push_block_values_for_round_;
  std::pair<blk_hash_t, bool> soft_voted_block_for_this_round_ =
      std::make_pair(NULL_BLOCK_HASH, false);
  std::unordered_map<vote_hash_t, Vote> next_votes_for_last_round_;
  std::vector<Vote> votes_;

  time_point round_clock_initial_datetime_;
  time_point now_;
  std::chrono::duration<double> duration_;
  long next_step_time_ms_ = 0;
  long elapsed_time_in_round_ms_ = 0;

  bool executed_pbft_block_ = false;
  bool have_executed_this_round_ = false;
  bool should_have_cert_voted_in_this_round_ = false;
  bool next_voted_soft_value_ = false;
  bool next_voted_null_block_hash_ = false;
  bool continue_finish_polling_state_ = false;
  bool go_finish_state_ = false;
  bool loop_back_finish_state_ = false;

  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;
  uint64_t pbft_round_last_next_votes_sync_ = 0;
  size_t pbft_step_last_next_votes_sync_ = 0;

  size_t pbft_last_observed_synced_queue_size_ = 0;

  std::atomic<uint64_t> dpos_period_;
  std::atomic<uint64_t> eligible_voter_count_;
  size_t sortition_threshold_ = 0;
  // 2t+1 minimum number of votes for consensus
  size_t TWO_T_PLUS_ONE = 0;

  std::string dag_genesis_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;
  mutable boost::shared_mutex round_access_;
  mutable boost::shared_mutex next_votes_access_;

  // TODO: will remove later, TEST CODE
  void countVotes_();

  std::shared_ptr<std::thread> monitor_votes_ = nullptr;
  std::atomic<bool> monitor_stop_ = true;
  size_t last_step_ = 0;
  time_point last_step_clock_initial_datetime_;
  time_point current_step_clock_initial_datetime_;
  // END TEST CODE

  std::atomic<uint64_t> num_executed_trx_ = 0;
  std::atomic<uint64_t> num_executed_blk_ = 0;
  dev::eth::Transactions transactions_tmp_buf_;

  LOG_OBJECTS_DEFINE;
  mutable boost::log::sources::severity_channel_logger<> log_nf_test_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_TEST")};
};

}  // namespace taraxa

#endif  // PBFT_MANAGER_H
