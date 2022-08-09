#pragma once

#include <string>
#include <thread>

#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft/node_face.hpp"
#include "pbft/period_data_queue.hpp"
#include "pbft/round.hpp"
#include "pbft/step/step.hpp"
#include "pbft/utils.hpp"

// #define MAX_WAIT_FOR_NEXT_VOTED_BLOCK_STEPS 20

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class FullNode;

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
 * There are 5 states in one PBFT round: propose state, filter state, certify state, finish state, and finish polling
 * state.
 * - Propose state: PBFT step 1. Generate a PBFT block and propose a vote on the block hash
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
 * - Propose state: Reset clock to 0
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
              std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
              std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
              std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain, secret_t node_sk,
              vrf_wrapper::vrf_sk_t vrf_sk, uint32_t max_levels_per_period = kMaxLevelsPerPeriod);
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
  void initialize();

  /**
   * @brief Check PBFT blocks syncing queue. If there are synced PBFT blocks in queue, push it to PBFT chain
   */
  void pushSyncedPbftBlocksIntoChain();

  /**
   * @brief Get PBFT round number
   * @return PBFT round
   */
  uint64_t getPbftRound() const;

  /**
   * @brief Set PBFT round number
   * @param round PBFT round
   */
  // void setPbftRound(uint64_t const round);

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
   * @brief Generate a vote
   * @param blockhash vote on PBFT block hash
   * @param type vote type
   * @param round PBFT round
   * @param step PBFT step
   * @return vote
   */
  std::shared_ptr<Vote> generateVote(blk_hash_t const &blockhash, PbftVoteType type, uint64_t round, size_t step);

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
  size_t getPbftCommitteeSize() const { return config_.committee_size; }

  /**
   * @brief Get PBFT lambda. PBFT lambda is a timer clock
   * @return PBFT lambda
   */
  uint64_t getPbftInitialLambda() const { return config_.lambda_ms_min; }

  /**
   * @note Used only for tests
   * @brief Go to next PBFT state
   */
  // void doNextState_();

  /**
   * @note Used only for tests
   * @brief Set last soft vote value of PBFT block hash
   * @param soft_voted_value soft vote value of PBFT block hash
   */
  // void setLastSoftVotedValue(blk_hash_t soft_voted_value) { updateLastSoftVotedValue_(soft_voted_value); }

  /**
   * @note Used only for tests
   * @brief Resume PBFT daemon
   */
  // void resume();

  /**
   * @note Used only for tests
   * @brief Resume PBFT daemon on single state
   */
  // void resumeSingleState();

  /**
   * @note Used only for tests
   * @brief Set maximum waiting time for soft vote value
   * @param wait_ms waiting time in milisecond
   */
  // void setMaxWaitForSoftVotedBlock_ms(uint64_t wait_ms);

  /**
   * @note Used only for tests
   * @brief Set maximum waiting time for next vote value
   * @param wait_ms waiting time in milisecond
   */
  // void setMaxWaitForNextVotedBlock_ms(uint64_t wait_ms);

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

  // DPOS
  /**
   * @brief Update DPOS period, total DPOS votes count, and node DPOS weighted votes count.
   */
  void updateDposState();

  /**
   * @brief Get node DPOS eligible votes count
   * @param addr node address
   * @return node DPOS eligible votes count
   */
  size_t dposEligibleVoteCount(addr_t const &addr);

  /**
   * @brief If node receives enough next voting votes on a forward PBFT round, set PBFT round to the round number.
   */
  void moveToRound(uint64_t round, std::optional<uint64_t> step = {});
  // bool resetRound_();

  /**
   * @brief PBFT daemon
   */
  void continuousOperation();

  /**
   * @brief move to next Step
   */
  // void nextStep_();

  /**
   * @brief If there are any synced PBFT blocks from peers, push the synced blocks in PBFT chain. Verify all received
   * incoming votes. If there are enough certify votes, push voting PBFT block in PBFT chain
   * @return true if there are enough certify votes voting on a new PBFT block, or PBFT goes to a forward round
   */
  bool stateOperations();

  /**
   * @brief Get PBFT sortition threshold
   * @param vote_type vote type
   * @return PBFT sortition threshold
   */
  uint64_t getThreshold(PbftVoteType vote_type) const;

  /**
   * @brief Only be able to send a syncing request per each PBFT round and step
   * @return true if the current PBFT round and step has sent a syncing request already
   */
  // bool syncRequestedAlreadyThisStep_() const;

  /**
   * @brief Send a syncing request to peer
   * @param reason syncing request reason
   * @param relevant_blk_hash relevant block hash
   */
  // void syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash);

  /**
   * @brief Check that there are all DAG blocks with correct ordering, total gas estimation is not greater than gas
   * limit, and PBFT block includes all reward votes.
   * @param pbft_block_hash PBFT block hash
   * @return true if pass verification
   */
  bool compareBlocksAndRewardVotes(const blk_hash_t &pbft_block_hash);

  /**
   * @brief Check that there are all DAG blocks with correct ordering, total gas estimation is not greater than gas
   * limit, and PBFT block include all reward votes.
   * @param pbft_block PBFT block
   * @return true with DAG blocks hashes in order if passed verification. Otherwise return false
   */
  std::pair<vec_blk_t, bool> compareBlocksAndRewardVotes(std::shared_ptr<PbftBlock> pbft_block);

  /**
   * @brief If there are enough certify votes, push the vote PBFT block in PBFT chain
   * @param cert_voted_block_hash PBFT block hash
   * @param current_round_cert_votes certify votes
   * @return true if push a new PBFT block in chain
   */
  bool pushCertVotedPbftBlockIntoChain(blk_hash_t const &cert_voted_block_hash,
                                       std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes);

  /**
   * @brief Final chain executes a finalized PBFT block
   * @param period_data PBFT block, cert votes, DAG blocks, and transactions
   * @param finalized_dag_blk_hashes DAG blocks hashes
   * @param sync it's true when it's last finalized block
   */
  void finalize(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes, bool sync = false);

  /**
   * @brief Push a new PBFT block into the PBFT chain
   * @param period_data PBFT block, cert votes for previous period, DAG blocks, and transactions
   * @param cert_votes cert votes for pbft block period
   * @param dag_blocks_order DAG blocks hashes
   * @return true if push a new PBFT block into the PBFT chain
   */
  bool pushPbftBlock(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes,
                     vec_blk_t &&dag_blocks_order = {});

  /**
   * @brief Update PBFT 2t+1 and PBFT sortition threshold
   */
  void updateTwoTPlusOneAndThreshold();

  /**
   * @brief Check PBFT is working on syncing or not
   * @return true if PBFT is working on syncing
   */
  // bool is_syncing_();

  /**
   * @brief Set initial time for voting value
   */
  void initializeVotedValueTimeouts();

  /**
   * @brief Check if previous round next voting value has been changed
   */
  void checkPreviousRoundNextVotedValueChange();

  /**
   * @brief Update soft voting PBFT block if there are enough soft voting votes
   * @return true if soft voting PBFT block updated
   */
  bool updateSoftVotedBlockForThisRound();

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
  std::shared_ptr<PbftBlock> getUnfinalizedBlock(blk_hash_t const &block_hash);

  std::atomic<bool> stopped_ = true;

  std::unique_ptr<std::thread> daemon_;

  std::shared_ptr<NodeFace> node_;
  const addr_t node_addr_;

  std::shared_ptr<Round> round_;

  // Period data for pbft block that is being currently cert voted for
  PeriodData period_data_;

  uint64_t max_wait_for_soft_voted_block_steps_ms_ = 30;

  // Was used for 1 unit test
  // uint64_t max_wait_for_next_voted_block_steps_ms_ = 30;

  blk_hash_t last_soft_voted_value_ = kNullBlockHash;
  uint64_t pbft_round_last_requested_sync_ = 0;
  size_t pbft_step_last_requested_sync_ = 0;

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

 private:
  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
