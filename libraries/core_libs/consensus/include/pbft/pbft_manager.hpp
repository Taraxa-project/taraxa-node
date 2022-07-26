#pragma once

#include <string>
#include <thread>

#include "common/types.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft/period_data_queue.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)
#define POLLING_INTERVAL_ms 100  // milliseconds...
#define MAX_STEPS 13             // Need to be a odd number
#define MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS 20
#define MAX_WAIT_FOR_NEXT_VOTED_BLOCK_STEPS 20

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class FullNode;

enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

enum PbftSyncRequestReason {
  missing_dag_blk = 1,
  invalid_cert_voted_block,
  invalid_soft_voted_block,
  exceeded_max_steps
};

/**
 * @brief PbftManager class is a daemon that is used to finalize a bench of directed acyclic graph (DAG) blocks by using
 * Practical Byzantine Fault Tolerance (PBFT) protocol
 *
 * According to paper "ALGORAND AGREEMENT Super Fast and Partition Resilient Byzantine Agreement
 * (https://eprint.iacr.org/2018/377.pdf)", implement PBFT manager for finalizing DAG blocks.
 *
 * There are 5 states in one PBFT round: proposal state, filter state, certify state, finish state, and finish polling
 * state.
 * - Proposal state: PBFT step 1. Generate a PBFT block and propose a vote on the block hash
 * - Filter state: PBFT step 2. Identify a leader block from all received proposed blocks for the current period by
 * using minimum Verifiable Random Function (VRF) output. Soft vote at the leader block hash. In filter state, don’t
 * need check vote value correction.
 * - Certify state: PBFT step 3. If receive enough soft votes, cert vote at the value. If receive enough cert votes,
 * finalize the PBFT block and push it to PBFT chain.
 * - Finish state: Happens at even number steps from step 4. Next vote at finishing value for the current PBFT round. If
 * node receives enough next voting votes, PBFT goes to next round.
 * - Finish polling state: Happens at odd number steps from step 5. Next vote at finishing value for the current PBFT
 * round. If node receives enough next voting votes, PBFT goes to next round.
 *
 * PBFT timing: All players keep a timer clock. The timer clock will reset to 0 at every new PBFT round. That doesn’t
 * require all players clocks to be synchronized; it only requires that they have the same clock speed.
 * - Proposal state: Reset clock to 0
 * - Filter state: Start at clock 2 lambda time
 * - Certify state: Start after filter state, clock is between 2 lambda and 4 lambda duration
 * - Finish state: Start at 4 lambda time, until receive enough next voting votes to go to next round
 * - Finish polling state: Start after first finish state. If node receives enough next voting votes within 2 lambda
 * duration, PBFT will go to next round. Otherwise that will go back to Finish state.
 */
class PbftManager : public std::enable_shared_from_this<PbftManager> {
 public:
  using time_point = std::chrono::system_clock::time_point;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;

  PbftManager(const PbftConfig &conf, const blk_hash_t &dag_genesis_block_hash, addr_t node_addr,
              std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
              std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
              std::shared_ptr<FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager, secret_t node_sk,
              vrf_sk_t vrf_sk, uint32_t max_levels_per_period = kMaxLevelsPerPeriod);
  ~PbftManager();
  PbftManager(const PbftManager &) = delete;
  PbftManager(PbftManager &&) = delete;
  PbftManager &operator=(const PbftManager &) = delete;
  PbftManager &operator=(PbftManager &&) = delete;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pinter
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Start PBFT daemon
   */
  void start();

  /**
   * @brief Stop PBFT daemon
   */
  void stop();

  /**
   * @brief Run PBFT daemon
   */
  void run();

  /**
   * @brief Initial PBFT states when node start PBFT
   */
  void initialState();

  /**
   * @brief Check PBFT blocks syncing queue. If there are synced PBFT blocks in queue, push it to PBFT chain
   */
  void pushSyncedPbftBlocksIntoChain();

  /**
   * @brief Get a DAG block period number
   * @param hash DAG block hash
   * @return true with DAG block period number if the DAG block has been finalized. Otherwise return false
   */
  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);

  /**
   * @brief Get PBFT round number
   * @return PBFT round
   */
  uint64_t getPbftRound() const;

  /**
   * @brief Get PBFT step number
   * @return PBFT step
   */
  uint64_t getPbftStep() const;

  /**
   * @brief Set PBFT round number
   * @param round PBFT round
   */
  void setPbftRound(uint64_t const round);

  /**
   * @brief Get PBFT sortition threshold. PBFT sortition threshold is minimum of between PBFT committee size and total
   * DPOS votes count
   * @return PBFT sortition threshold
   */
  size_t getSortitionThreshold() const;

  /**
   * @brief Get 2t+1. 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   * @return 2t+1
   */
  size_t getTwoTPlusOne() const;

  /**
   * @brief Set PBFT step
   * @param pbft_step PBFT step
   */
  void setPbftStep(size_t const pbft_step);

  /**
   * @brief Generate PBFT block, push into unverified queue, and broadcast to peers
   * @param prev_blk_hash previous PBFT block hash
   * @param anchor_hash proposed DAG pivot block hash for finalization
   * @param order_hash the hash of all DAG blocks include in the PBFT block
   * @return PBFT block hash
   */
  blk_hash_t generatePbftBlock(const blk_hash_t &prev_blk_hash, const blk_hash_t &anchor_hash,
                               const blk_hash_t &order_hash);

  /**
   * @brief Generate a vote
   * @param blockhash vote on PBFT block hash
   * @param type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @return vote
   */
  std::shared_ptr<Vote> generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t period, uint64_t round,
                                     size_t step);

  /**
   * @brief Get total DPOS votes count
   * @return total DPOS votes count
   */
  size_t getDposTotalVotesCount() const;

  /**
   * @brief Get node DPOS weighted votes count
   * @return node DPOS weighted votes count
   */
  size_t getDposWeightedVotesCount() const;

  /**
   * @brief Get PBFT blocks synced period
   * @return PBFT blocks synced period
   */
  uint64_t pbftSyncingPeriod() const;

  /**
   * @brief Get PBFT blocks syncing queue size
   * @return PBFT syncing queue size
   */
  size_t periodDataQueueSize() const;

  /**
   * @brief Returns true if queue is empty
   * @return
   */
  bool periodDataQueueEmpty() const;

  /**
   * @brief Push synced period data in syncing queue
   * @param block synced period data from peer
   * @param current_block_cert_votes cert votes for PeriodData pbft block period
   * @param node_id peer node ID
   */
  void periodDataQueuePush(PeriodData &&period_data, dev::p2p::NodeID const &node_id,
                           std::vector<std::shared_ptr<Vote>> &&current_block_cert_votes);

  /**
   * @brief Get last pbft block hash from queue or if queue empty, from chain
   * @return last block hash
   */
  blk_hash_t lastPbftBlockHashFromQueueOrChain();

  // Notice: Test purpose
  // TODO: Add a check for some kind of guards to ensure these are only called from within a test
  /**
   * @brief Set PBFT sortition threshold
   * @param sortition_threshold PBFT sortition threshold
   */
  void setSortitionThreshold(size_t const sortition_threshold);

  /**
   * @brief Get PBFT committee size
   * @return PBFT committee size
   */
  size_t getPbftCommitteeSize() const { return COMMITTEE_SIZE; }

  /**
   * @brief Get PBFT lambda. PBFT lambda is a timer clock
   * @return PBFT lambda
   */
  u_long getPbftInitialLambda() const { return LAMBDA_ms_MIN; }

  /**
   * @brief Set last soft vote value of PBFT block hash
   * @param soft_voted_value soft vote value of PBFT block hash
   */
  void setLastSoftVotedValue(blk_hash_t soft_voted_value);

  /**
   * @brief Resume PBFT daemon. Only to be used for unit tests
   */
  void resume();

  /**
   * @brief Resume PBFT daemon on single state. Only to be used for unit tests
   */
  void resumeSingleState();

  /**
   * @brief Set maximum waiting time for soft vote value. Only to be used for unit tests
   * @param wait_ms waiting time in milisecond
   */
  void setMaxWaitForSoftVotedBlock_ms(uint64_t wait_ms);

  /**
   * @brief Set maximum waiting time for next vote value. Only to be used for unit tests
   * @param wait_ms waiting time in milisecond
   */
  void setMaxWaitForNextVotedBlock_ms(uint64_t wait_ms);

  /**
   * @brief Calculate DAG blocks ordering hash
   * @param dag_block_hashes DAG blocks hashes
   * @param trx_hashes transactions hashes
   * @return DAG blocks ordering hash
   */
  static blk_hash_t calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes,
                                       const std::vector<trx_hash_t> &trx_hashes);

  /**
   * @brief Calculate DAG blocks ordering hash
   * @param dag_blocks DAG blocks
   * @param transactions transactions
   * @return DAG blocks ordering hash
   */
  static blk_hash_t calculateOrderHash(const std::vector<DagBlock> &dag_blocks, const SharedTransactions &transactions);

  /**
   * @brief Check a block weight of gas estimation
   * @param period_data period data
   * @return true if total weight of gas estimation is less or equal to gas limit. Otherwise return false
   */
  bool checkBlockWeight(const PeriodData &period_data) const;

  /**
   * @brief Get finalized DPOS period
   * @return DPOS period
   */
  uint64_t getFinalizedDPOSPeriod() const { return dpos_period_; }

  blk_hash_t getLastPbftBlockHash();

  /**
   * @brief Get only include reward votes that are list in PBFT block
   * @param reward_votes_hashes reward votes hashes are list in PBFT block
   * @return reward votes that are list in PBFT block
   */
  std::vector<std::shared_ptr<Vote>> getRewardVotesInBlock(const std::vector<vote_hash_t> &reward_votes_hashes);

  /**
   * @brief Validates vote
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVote(const std::shared_ptr<Vote> &vote) const;

 private:
  // DPOS
  /**
   * @brief Update DPOS period, total DPOS votes count, and node DPOS weighted votes count.
   */
  void updateDposState_();

  /**
   * @brief Get node DPOS eligible votes count
   * @param addr node address
   * @return node DPOS eligible votes count
   */
  size_t dposEligibleVoteCount_(addr_t const &addr);

  /**
   * @brief Reset PBFT step to 1
   */
  void resetStep_();

  /**
   * @brief If node receives enough next voting votes on a forward PBFT round, set PBFT round to the round number.
   * @return true if PBFT sets round to a forward round number
   */
  bool resetRound_();

  /**
   * @brief Time to sleep for PBFT protocol
   */
  void sleep_();

  /**
   * @brief PBFT daemon
   */
  void continuousOperation_();

  /**
   * @brief Go to next PBFT state. Only to be used for unit tests
   */
  void doNextState_();

  /**
   * @brief Set next PBFT state
   */
  void setNextState_();

  /**
   * @brief Set PBFT filter state
   */
  void setFilterState_();

  /**
   * @brief Set PBFT certify state
   */
  void setCertifyState_();

  /**
   * @brief Set PBFT finish state
   */
  void setFinishState_();

  /**
   * @brief Set PBFT finish polling state
   */
  void setFinishPollingState_();

  /**
   * @brief Set back to PBFT finish state from PBFT finish polling state
   */
  void loopBackFinishState_();

  /**
   * @brief If there are any synced PBFT blocks from peers, push the synced blocks in PBFT chain. Verify all received
   * incoming votes. If there are enough certify votes, push voting PBFT block in PBFT chain
   * @return true if there are enough certify votes voting on a new PBFT block, or PBFT goes to a forward round
   */
  bool stateOperations_();

  /**
   * @brief PBFT proposal state. PBFT step 1. Propose a PBFT block and place a proposal vote on the block hash.
   */
  void proposeBlock_();

  /**
   * @brief PBFT filter state. PBFT step 2. Identify a leader block from all received proposed blocks for the current
   * period, and place a soft vote at the leader block hash.
   */
  void identifyBlock_();

  /**
   * @brief PBFT certify state. PBFT step 3. If receive enough soft votes and pass verification, place a cert vote at
   * the value.
   */
  void certifyBlock_();

  /**
   * @brief PBFT finish state. Happens at even number steps from step 4. Place a next vote at finishing value for the
   * current PBFT round.
   */
  void firstFinish_();

  /**
   * @brief PBFT finish polling state: Happens at odd number steps from step 5. Place a next vote at finishing value for
   * the current PBFT round.
   */
  void secondFinish_();

  /**
   * @brief Place a vote, save it in the verified votes queue, and gossip to peers
   * @param blockhash vote on PBFT block hash
   * @param vote_type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @return vote weight
   */
  size_t placeVote_(blk_hash_t const &blockhash, PbftVoteTypes vote_type, uint64_t period, uint64_t round, size_t step);

  /**
   * @brief Get current (based on the latest finalized block) PBFT sortition threshold
   * @param vote_type vote type
   * @return PBFT sortition threshold
   */
  uint64_t getCurrentPbftSortitionThreshold(PbftVoteTypes vote_type) const;

  /**
   * @brief Get PBFT sortition threshold for specific period
   * @param vote_type vote type
   * @param pbft_period pbft period
   * @return PBFT sortition threshold
   */
  uint64_t getPbftSortitionThreshold(PbftVoteTypes vote_type, uint64_t pbft_period) const;

  /**
   * @brief Propose a new PBFT block
   * @param propose_period PBFT propose period
   * @return proposed PBFT block hash
   */
  blk_hash_t proposePbftBlock_(uint64_t propose_period);

  /**
   * @brief Identify a leader block from all received proposed PBFT blocks for the current round by using minimum
   * Verifiable Random Function (VRF) output. In filter state, don’t need check vote value correction.
   * @return optional(pair<PBFT leader block hash, PBFT leader period>)
   */
  std::optional<std::pair<blk_hash_t, uint64_t>> identifyLeaderBlock_();

  /**
   * @brief Calculate the lowest hash of a vote by vote weight
   * @param vote vote
   * @return lowest hash of a vote
   */
  h256 getProposal(const std::shared_ptr<Vote> &vote) const;

  /**
   * @brief Only be able to send a syncing request per each PBFT round and step
   * @return true if the current PBFT round and step has sent a syncing request already
   */
  bool syncRequestedAlreadyThisStep_() const;

  /**
   * @brief Send a syncing request to peer
   * @param reason syncing request reason
   * @param relevant_blk_hash relevant block hash
   */
  void syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash);

  /**
   * @brief Only be able to broadcast one time of previous round next voting votes per each PBFT round and step
   * @return true if the current PBFT round and step has broadcasted previous round next voting votes already
   */
  bool broadcastAlreadyThisStep_() const;

  /**
   * @brief Check that there are all DAG blocks with correct ordering, total gas estimation is not greater than gas
   * limit, and PBFT block includes all reward votes.
   * @param pbft_block_hash PBFT block hash
   * @return true if pass verification
   */
  bool compareBlocksAndRewardVotes_(const blk_hash_t &pbft_block_hash);

  /**
   * @brief Check that there are all DAG blocks with correct ordering, total gas estimation is not greater than gas
   * limit, and PBFT block include all reward votes.
   * @param pbft_block PBFT block
   * @return true with DAG blocks hashes in order if passed verification. Otherwise return false
   */
  std::pair<vec_blk_t, bool> compareBlocksAndRewardVotes_(std::shared_ptr<PbftBlock> pbft_block);

  /**
   * @brief If there are enough certify votes, push the vote PBFT block in PBFT chain
   * @param cert_voted_block_hash PBFT block hash
   * @param current_round_cert_votes certify votes
   * @return true if push a new PBFT block in chain
   */
  bool pushCertVotedPbftBlockIntoChain_(blk_hash_t const &cert_voted_block_hash,
                                        std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes);

  /**
   * @brief Final chain executes a finalized PBFT block
   * @param period_data PBFT block, cert votes, DAG blocks, and transactions
   * @param finalized_dag_blk_hashes DAG blocks hashes
   * @param sync it's true when it's last finalized block
   */
  void finalize_(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes, bool sync = false);

  /**
   * @brief Push a new PBFT block into the PBFT chain
   * @param period_data PBFT block, cert votes for previous period, DAG blocks, and transactions
   * @param cert_votes cert votes for pbft block period
   * @param dag_blocks_order DAG blocks hashes
   * @return true if push a new PBFT block into the PBFT chain
   */
  bool pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes,
                      vec_blk_t &&dag_blocks_order = {});

  /**
   * @brief Update PBFT 2t+1 and PBFT sortition threshold
   */
  void updateTwoTPlusOneAndThreshold_();

  /**
   * @brief Check PBFT is working on syncing or not
   * @return true if PBFT is working on syncing
   */
  bool is_syncing_();

  /**
   * @brief Terminate the next voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpNextVotedBlock_();

  /**
   * @brief Terminate the soft voting value of the PBFT block hash
   * @return true if terminate the vote value successfully
   */
  bool giveUpSoftVotedBlock_();

  /**
   * @brief Set initial time for voting value
   */
  void initializeVotedValueTimeouts_();

  /**
   * @brief Update last soft voting value
   * @param new_soft_voted_value soft voting value
   */
  void updateLastSoftVotedValue_(blk_hash_t const new_soft_voted_value);

  /**
   * @brief Check if previous round next voting value has been changed
   */
  void checkPreviousRoundNextVotedValueChange_();

  /**
   * @brief Update soft voting PBFT block if there are enough soft voting votes
   * @return true if soft voting PBFT block updated
   */
  bool updateSoftVotedBlockForThisRound_();

  /**
   * @brief Process synced PBFT blocks if PBFT syncing queue is not empty
   * @return period data with cert votes for the current period
   */
  std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>> processPeriodData();

  /**
   * @brief Get the unfinalized PBFT block for current period
   * @param block_hash PBFT block hash
   * @return PBFT block
   */
  std::shared_ptr<PbftBlock> getUnfinalizedBlock_(blk_hash_t const &block_hash);

  std::atomic<bool> stopped_ = true;

  // Ensures that only one PBFT block per period can be proposed
  blk_hash_t proposed_block_hash_ = NULL_BLOCK_HASH;

  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<NextVotesManager> next_votes_manager_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;

  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;

  u_long const LAMBDA_ms_MIN;
  u_long LAMBDA_ms = 0;
  u_long LAMBDA_backoff_multiple = 1;
  const u_long kMaxLambda = 60000;  // in ms, max lambda is 1 minutes

  std::default_random_engine random_engine_{std::random_device{}()};

  const size_t COMMITTEE_SIZE;
  const size_t NUMBER_OF_PROPOSERS;
  const size_t DAG_BLOCKS_SIZE;
  const size_t GHOST_PATH_MOVE_BACK;
  bool RUN_COUNT_VOTES;  // TODO: Only for test, need remove later

  PbftStates state_ = value_proposal_state;

  std::atomic<uint64_t> round_ = 1;
  size_t step_ = 1;
  size_t startingStepInRound_ = 1;

  blk_hash_t own_starting_value_for_round_ = NULL_BLOCK_HASH;
  blk_hash_t soft_voted_block_for_this_round_ = NULL_BLOCK_HASH;

  // Period data for pbft block that is being currently cert voted for
  PeriodData period_data_;

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

  std::atomic<uint64_t> dpos_period_;
  std::atomic<size_t> dpos_votes_count_;
  std::atomic<size_t> weighted_votes_count_;

  size_t sortition_threshold_ = 0;
  // 2t+1 minimum number of votes for consensus
  size_t TWO_T_PLUS_ONE = 0;

  const blk_hash_t dag_genesis_block_hash_;

  const PbftConfig &config_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  PeriodDataQueue sync_queue_;

  const uint32_t max_levels_per_period_;

  /**
   * @brief Count how many votes in the current PBFT round and step. This is only for testing purpose
   */
  void countVotes_() const;

  std::shared_ptr<std::thread> monitor_votes_ = nullptr;
  std::atomic<bool> monitor_stop_ = true;
  size_t last_step_ = 0;
  time_point last_step_clock_initial_datetime_;
  time_point current_step_clock_initial_datetime_;
  // END TEST CODE

  LOG_OBJECTS_DEFINE
  mutable logger::Logger log_nf_test_{logger::createLogger(taraxa::logger::Verbosity::Info, "PBFT_TEST", node_addr_)};
};

/** @}*/

}  // namespace taraxa
