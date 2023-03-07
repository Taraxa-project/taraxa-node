#pragma once

#include <string>
#include <thread>

#include "common/types.hpp"
#include "config/config.hpp"
#include "final_chain/final_chain.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft/period_data_queue.hpp"
#include "pbft/proposed_blocks.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class FullNode;

enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

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

  PbftManager(const PbftConfig &conf, const blk_hash_t &dag_genesis_block_hash, addr_t node_addr,
              std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
              std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain, secret_t node_sk,
              uint32_t max_levels_per_period);
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
   * @brief Get a DAG block period number
   * @param hash DAG block hash
   * @return true with DAG block period number if the DAG block has been finalized. Otherwise return false
   */
  std::pair<bool, PbftPeriod> getDagBlockPeriod(const blk_hash_t &hash);

  /**
   * @brief Get current PBFT period number
   * @return current PBFT period
   */
  PbftPeriod getPbftPeriod() const;

  /**
   * @brief Get current PBFT round number
   * @return current PBFT round
   */
  PbftRound getPbftRound() const;

  /**
   * @brief Get PBFT round & period number
   * @return <PBFT round, PBFT period>
   */
  // TODO: exchange round <-> period
  std::pair<PbftRound, PbftPeriod> getPbftRoundAndPeriod() const;

  /**
   * @brief Get PBFT step number
   * @return PBFT step
   */
  PbftStep getPbftStep() const;

  /**
   * @brief Set PBFT round number
   * @param round PBFT round
   */
  void setPbftRound(PbftRound round);

  /**
   * @brief Set PBFT step
   * @param pbft_step PBFT step
   */
  void setPbftStep(PbftStep pbft_step);

  /**
   * @brief Generate PBFT block, push into unverified queue, and broadcast to peers
   * @param propose_period
   * @param prev_blk_hash previous PBFT block hash
   * @param anchor_hash proposed DAG pivot block hash for finalization
   * @param order_hash the hash of all DAG blocks include in the PBFT block
   * @return optional<pair<PBFT block, PBFT block reward votes>>
   */
  std::optional<std::pair<std::shared_ptr<PbftBlock>, std::vector<std::shared_ptr<Vote>>>> generatePbftBlock(
      PbftPeriod propose_period, const blk_hash_t &prev_blk_hash, const blk_hash_t &anchor_hash,
      const blk_hash_t &order_hash);

  /**
   * @brief Get current total DPOS votes count
   * @return current total DPOS votes count if successful, otherwise (due to non-existent data for pbft_period) empty
   * optional
   */
  std::optional<uint64_t> getCurrentDposTotalVotesCount() const;

  /**
   * @brief Get current node DPOS votes count
   * @return node current DPOS votes count if successful, otherwise (due to non-existent data for pbft_period) empty
   * optional
   */
  std::optional<uint64_t> getCurrentNodeVotesCount() const;

  /**
   * @brief Get PBFT blocks synced period
   * @return PBFT blocks synced period
   */
  PbftPeriod pbftSyncingPeriod() const;

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

  /**
   * @brief Add rebuild DB with provided data
   * @param block period data
   * @param current_block_cert_votes cert votes for PeriodData pbft block period
   */
  void addRebuildDBPeriodData(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&current_block_cert_votes);

  /**
   * @brief Get PBFT lambda. PBFT lambda is a timer clock
   * @return PBFT lambda
   */
  std::chrono::milliseconds getPbftInitialLambda() const { return LAMBDA_ms_MIN; }

  /**
   * @brief Calculate DAG blocks ordering hash
   * @param dag_block_hashes DAG blocks hashes
   * @return DAG blocks ordering hash
   */
  static blk_hash_t calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes);

  /**
   * @brief Calculate DAG blocks ordering hash
   * @param dag_blocks DAG blocks
   * @return DAG blocks ordering hash
   */
  static blk_hash_t calculateOrderHash(const std::vector<DagBlock> &dag_blocks);

  /**
   * @brief Reorder transactions data if DAG reordering caused transactions with same sender to have nonce in incorrect
   * order. Reordering is deterministic so that same order is produced on any node on any platform
   * @param transactions transactions to reorder
   */
  static void reorderTransactions(SharedTransactions &transactions);

  /**
   * @brief Check a block weight of gas estimation
   * @param dag_blocks dag blocks
   * @return true if total weight of gas estimation is less or equal to gas limit. Otherwise return false
   */
  bool checkBlockWeight(const std::vector<DagBlock> &dag_blocks) const;

  blk_hash_t getLastPbftBlockHash();

  /**
   * @brief Push proposed block into the proposed_blocks_ in case it is not there yet
   *
   * @param proposed_block
   * @param propose_vote
   */
  void processProposedBlock(const std::shared_ptr<PbftBlock> &proposed_block,
                            const std::shared_ptr<Vote> &propose_vote);

  // **** Notice: functions used only in tests ****
  // TODO: Add a check for some kind of guards to ensure these are only called from within a test
  /**
   * @brief Resume PBFT daemon. Only to be used for unit tests
   */
  void resume();

  /**
   * @brief Get a proposed PBFT block based on specified period and block hash
   * @param period
   * @param block_hash
   * @return std::shared_ptr<PbftBlock>
   */
  std::shared_ptr<PbftBlock> getPbftProposedBlock(PbftPeriod period, const blk_hash_t &block_hash) const;

  /**
   * @brief Get PBFT committee size
   * @return PBFT committee size
   */
  size_t getPbftCommitteeSize() const { return config_.committee_size; }

  /**
   * @brief Broadcast or rebroadcast current round soft votes, previous round next votes and reward votes
   * @param rebroadcast
   */
  void broadcastVotes(bool rebroadcast);

 private:
  /**
   * @brief Check PBFT blocks syncing queue. If there are synced PBFT blocks in queue, push it to PBFT chain
   */
  void pushSyncedPbftBlocksIntoChain();

  // DPOS
  /**
   * @brief wait for DPOS period finalization
   */
  void waitForPeriodFinalization();

  /**
   * @brief Reset PBFT step to 1
   */
  void resetStep();

  /**
   * @brief If node receives 2t+1 next votes for some block(including kNullBlockHash), advance round to + 1.
   * @return true if PBFT round advanced, otherwise false
   */
  bool advanceRound();

  /**
   * @brief If node receives 2t+1 cert votes for some valid block and pushes it to the chain, advance period to + 1.
   * @return true if PBFT period advanced, otherwise false
   */
  bool advancePeriod();

  /**
   * @brief Check if there is 2t+1 cert votes for some valid block, if yes - push it into the chain
   * @return true if new cert voted block was pushed into the chain, otheriwse false
   */
  bool tryPushCertVotesBlock();

  /**
   * @brief Resets pbft consensus: current pbft round is set to round, step is set to the beginning value
   * @param round
   */
  void resetPbftConsensus(PbftRound round);

  /**
   * @param start_time
   * @return elapsed time in ms from provided start_time
   */
  std::chrono::milliseconds elapsedTimeInMs(const time_point &start_time);

  /**
   * @brief Time to sleep for PBFT protocol
   */
  void sleep_();

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
   * @brief Place (gossip) vote
   * @param vote
   * @param log_vote_id vote identifier for log msg
   * @param voted_block voted block object - should be == vote->voted_block. In case we dont have block object, nullptr
   *                    is provided
   */
  bool placeVote(const std::shared_ptr<Vote> &vote, std::string_view log_vote_id,
                 const std::shared_ptr<PbftBlock> &voted_block);

  /**
   * @brief Generate propose vote for provided block place (gossip) it
   *
   * @param proposed_block
   * @param reward_votes for proposed_block
   * @return true if successful, otherwise false
   */
  bool genAndPlaceProposeVote(const std::shared_ptr<PbftBlock> &proposed_block,
                              std::vector<std::shared_ptr<Vote>> &&reward_votes);

  /**
   * @brief Gossips newly generated vote to the other peers
   *
   * @param vote
   * @param voted_block
   * @return true if successful, otherwise false
   */
  void gossipNewVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &voted_block);

  /**
   * @brief Propose a new PBFT block
   * @return optional<pair<PBFT block, PBFT block reward votes>> in case new block was proposed, otherwise empty
   * optional
   */
  std::optional<std::pair<std::shared_ptr<PbftBlock>, std::vector<std::shared_ptr<Vote>>>> proposePbftBlock();

  /**
   * @brief Identify a leader block from all received proposed PBFT blocks for the current round by using minimum
   * Verifiable Random Function (VRF) output. In filter state, don’t need check vote value correction.
   * @param round current pbft round
   * @param period new pbft period (period == chain_size + 1)
   * @return shared_ptr to leader identified leader block
   */
  // TODO: exchange round <-> period
  std::shared_ptr<PbftBlock> identifyLeaderBlock_(PbftRound round, PbftPeriod period);

  /**
   * @brief Calculate the lowest hash of a vote by vote weight
   * @param vote vote
   * @return lowest hash of a vote
   */
  h256 getProposal(const std::shared_ptr<Vote> &vote) const;

  /**
   * @brief Validates pbft block. It checks if:
   *        - pbft_block's previous pbft block hash == node's latest finalized pbft block
   *        - node has all DAG blocks with correct ordering,
   *        - node has all reward votes
   *        - total gas estimation is not greater than gas limit
   * @param pbft_block PBFT block
   * @return true if pbft block is valid, otherwise false
   */
  bool validatePbftBlock(const std::shared_ptr<PbftBlock> &pbft_block) const;

  /**
   * @brief If there are enough certify votes, push the vote PBFT block in PBFT chain
   * @param pbft_block PBFT block
   * @param current_round_cert_votes certify votes
   * @return true if push a new PBFT block in chain
   */
  bool pushCertVotedPbftBlockIntoChain_(const std::shared_ptr<PbftBlock> &pbft_block,
                                        std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes);

  /**
   * @brief Final chain executes a finalized PBFT block
   * @param period_data PBFT block, cert votes, DAG blocks, and transactions
   * @param finalized_dag_blk_hashes DAG blocks hashes
   * @param synchronous_processing wait for block finalization to finish
   */
  void finalize_(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes,
                 bool synchronous_processing = false);

  /**
   * @brief Push a new PBFT block into the PBFT chain
   * @param period_data PBFT block, cert votes for previous period, DAG blocks, and transactions
   * @param cert_votes cert votes for pbft block period
   * @return true if push a new PBFT block into the PBFT chain
   */
  bool pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes);

  /**
   * @brief Get valid proposed pbft block. It will retrieve block from proposed_blocks and then validate it if not
   *        already validated
   *
   * @param period
   * @param block_hash
   * @return valid proposed pbft block or nullptr
   */
  std::shared_ptr<PbftBlock> getValidPbftProposedBlock(PbftPeriod period, const blk_hash_t &block_hash);

  /**
   * @brief Process synced PBFT blocks if PBFT syncing queue is not empty
   * @return period data with cert votes for the current period
   */
  std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>> processPeriodData();

  /**
   * @brief Validates PBFT block cert votes
   * @param pbft_block
   * @param cert_votes
   *
   * @return true if there is enough(2t+1) votes and all of them are valid, otherwise false
   */
  bool validatePbftBlockCertVotes(const std::shared_ptr<PbftBlock> pbft_block,
                                  const std::vector<std::shared_ptr<Vote>> &cert_votes) const;

  /**
   * @param period
   * @return true if node can participate in consensus - is dpos eligible to vote and create blocks for specified period
   */
  bool canParticipateInConsensus(PbftPeriod period) const;

  /**
   * @brief Prints all votes generated by node in current round
   */
  void printVotingSummary() const;

  std::atomic<bool> stopped_ = true;

  // Multiple proposed pbft blocks could have same dag block anchor at same period so this cache improves retrieval of
  // dag block order for specific anchor
  mutable std::unordered_map<blk_hash_t, std::vector<DagBlock>> anchor_dag_block_order_cache_;

  std::unique_ptr<std::thread> daemon_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;

  const addr_t node_addr_;
  const secret_t node_sk_;

  const std::chrono::milliseconds LAMBDA_ms_MIN;
  std::chrono::milliseconds LAMBDA_ms{0};
  uint64_t LAMBDA_backoff_multiple = 1;
  const std::chrono::milliseconds kMaxLambda{60000};  // in ms, max lambda is 1 minutes

  const uint32_t kBroadcastVotesLambdaTime = 20;
  const uint32_t kRebroadcastVotesLambdaTime = 60;
  uint32_t broadcast_votes_counter_ = 1;
  uint32_t rebroadcast_votes_counter_ = 1;

  std::default_random_engine random_engine_{std::random_device{}()};

  PbftStates state_ = value_proposal_state;
  std::atomic<PbftRound> round_ = 1;
  PbftStep step_ = 1;
  PbftStep startingStepInRound_ = 1;

  // Block that node cert voted
  std::optional<std::shared_ptr<PbftBlock>> cert_voted_block_for_round_{};

  // All broadcasted votes created by a node in current round - just for summary logging purposes
  std::map<blk_hash_t, std::vector<PbftStep>> current_round_broadcasted_votes_;

  time_point current_round_start_datetime_;
  time_point second_finish_step_start_datetime_;
  std::chrono::milliseconds next_step_time_ms_{0};

  bool executed_pbft_block_ = false;
  bool already_next_voted_value_ = false;
  bool already_next_voted_null_block_hash_ = false;
  bool go_finish_state_ = false;
  bool loop_back_finish_state_ = false;

  const blk_hash_t dag_genesis_block_hash_;

  const PbftConfig &config_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  PeriodDataQueue sync_queue_;

  // Proposed blocks based on received propose votes
  ProposedBlocks proposed_blocks_;

  const uint32_t max_levels_per_period_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
