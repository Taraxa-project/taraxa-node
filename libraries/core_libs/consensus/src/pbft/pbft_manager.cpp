#include "pbft/pbft_manager.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "config/version.hpp"
#include "dag/dag.hpp"
#include "dag/dag_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "pbft/period_data.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kPollingIntervalMs{100};
constexpr PbftStep kMaxSteps{13};  // Need to be a odd number

PbftManager::PbftManager(const FullNodeConfig &conf, std::shared_ptr<DbStorage> db,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                         std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                         std::shared_ptr<final_chain::FinalChain> final_chain,
                         std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_mgr)
    : db_(std::move(db)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      pillar_chain_mgr_(std::move(pillar_chain_mgr)),
      kSyncingThreadPoolSize(std::thread::hardware_concurrency() / 2),
      sync_thread_pool_(std::make_shared<util::ThreadPool>(kSyncingThreadPoolSize)),
      rounds_count_dynamic_lambda_(0),
      dynamic_lambda_(conf.genesis.state.hardforks.cacti_hf.lambda_max),
      dag_genesis_block_hash_(conf.genesis.dag_genesis_block.getHash()),
      kGenesisConfig(conf.genesis),
      proposed_blocks_(db_),
      eligible_wallets_(conf.wallets) {
  // Use first wallet as default node_addr
  const auto &node_addr = dev::toAddress(conf.getFirstWallet().node_secret);
  LOG_OBJECTS_CREATE("PBFT_MGR");

  for (auto period = final_chain_->lastBlockNumber() + 1, curr_period = pbft_chain_->getPbftChainSize();
       period <= curr_period; ++period) {
    auto period_data = db_->getPeriodData(period);
    if (!period_data.has_value()) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }

    if (period_data->pbft_blk->getPeriod() != period) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << period_data->pbft_blk->getBlockHash()
                   << " has different period " << period_data->pbft_blk->getPeriod()
                   << " in block data than in block order db: " << period;
      assert(false);
    }

    // We need this section because votes need to be verified for reward distribution
    for (const auto &v : period_data->previous_block_cert_votes) {
      vote_mgr_->validateVote(v);
    }

    uint32_t blocks_per_year{0};
    // Dynamic lambda was introduced in cacti hardfork -> it affects the number of blocks generated per year, which
    // affects rewards distribution
    if (kGenesisConfig.state.hardforks.isOnCactiHardfork(period)) {
      auto period_lambda = db_->getPeriodLambda(period, true);
      if (!period_lambda.has_value()) {
        LOG(log_er_) << "DB corrupted - no dynamic lambda saved for period " << period;
        assert(false);
      }

      blocks_per_year = kGenesisConfig.calcBlocksPerYear(
          *period_lambda,
          kGenesisConfig.state.hardforks.cacti_hf
              .consensus_delay /* approx time it takes to receive 2t+1 soft and cert votes after 2*lambda */);
    } else {
      blocks_per_year = kGenesisConfig.state.dpos.blocks_per_year;
    }

    finalize_(std::move(*period_data), db_->getFinalizedDagBlockHashesByPeriod(period), blocks_per_year,
              period == curr_period);
  }

  PbftPeriod start_period = 1;
  const auto recently_finalized_transactions_periods =
      kRecentlyFinalizedTransactionsFactor * final_chain_->delegationDelay();
  if (pbft_chain_->getPbftChainSize() > recently_finalized_transactions_periods) {
    start_period = pbft_chain_->getPbftChainSize() - recently_finalized_transactions_periods;
  }
  for (PbftPeriod period = start_period; period <= pbft_chain_->getPbftChainSize(); period++) {
    auto period_data = db_->getPeriodData(period);
    if (!period_data.has_value()) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    trx_mgr_->initializeRecentlyFinalizedTransactions(*period_data);
  }

  // Initialize PBFT status
  initialState();

  // Update wallets eligibility, call after initialState (waitForPeriodFinalization)
  eligible_wallets_.updateWalletsEligibility(pbft_chain_->getPbftChainSize(), final_chain_);

  // Note: processPillarBlock must be called after eligible_wallets_.updateWalletsEligibility
  auto current_pbft_period = pbft_chain_->getPbftChainSize();
  if (kGenesisConfig.state.hardforks.ficus_hf.isPillarBlockPeriod(current_pbft_period)) {
    const auto current_pillar_block = pillar_chain_mgr_->getCurrentPillarBlock();
    // There is a race condition where pbt block could have been saved and node stopped before saving pillar block
    if (current_pbft_period ==
        current_pillar_block->getPeriod() + kGenesisConfig.state.hardforks.ficus_hf.pillar_blocks_interval)
      LOG(log_er_) << "Pillar block was not processed before restart, current period: " << current_pbft_period
                   << ", current pillar block period: " << current_pillar_block->getPeriod();
    processPillarBlock(current_pbft_period);
  }
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  daemon_ = std::make_unique<std::thread>([this]() { run(); });
  LOG(log_dg_) << "PBFT daemon initiated ...";
}

void PbftManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  {
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.notify_all();
  }

  daemon_->join();
  final_chain_->stop();

  LOG(log_dg_) << "PBFT daemon terminated ...";
}

/* When a node starts up it has to sync to the current phase (type of block
 * being generated) and step (within the block generation round)
 * Five step loop for block generation over three phases of blocks
 * User's credential, sigma_i_p for a round p is sig_i(R, p)
 * Leader l_i_p = min ( H(sig_j(R,p) ) over set of j in S_i where S_i is set of
 * users from which have received valid round p credentials
 */
void PbftManager::run() {
  while (!stopped_) {
    if (stateOperations_()) {
      continue;
    }

    // PBFT states
    switch (state_) {
      case value_proposal_state:
        proposeBlock_();
        setFilterState_();
        break;
      case filter_state:
        identifyBlock_();
        setCertifyState_();
        break;
      case certify_state:
        certifyBlock_();
        if (go_finish_state_) {
          setFinishState_();
        } else {
          next_step_time_ms_ += kPollingIntervalMs;
        }
        break;
      case finish_state:
        firstFinish_();
        setFinishPollingState_();
        break;
      case finish_polling_state:
        secondFinish_();
        if (loop_back_finish_state_) {
          loopBackFinishState_();

          // Print voting summary for current round
          printVotingSummary();
        } else {
          next_step_time_ms_ += kPollingIntervalMs;
        }
        break;
      default:
        LOG(log_er_) << "Unknown PBFT state " << state_;
        assert(false);
    }

    LOG(log_tr_) << "next step time(ms): " << next_step_time_ms_.count() << ", step " << step_;
    sleep_();
  }
}

std::pair<bool, PbftPeriod> PbftManager::getDagBlockPeriod(const blk_hash_t &hash) {
  std::pair<bool, uint64_t> res;
  auto value = db_->getDagBlockPeriod(hash);
  if (value == nullptr) {
    res.first = false;
  } else {
    res.first = true;
    res.second = value->first;
  }
  return res;
}

PbftPeriod PbftManager::getPbftPeriod() const { return pbft_chain_->getPbftChainSize() + 1; }

PbftRound PbftManager::getPbftRound() const { return round_; }

std::pair<PbftRound, PbftPeriod> PbftManager::getPbftRoundAndPeriod() const {
  return {getPbftRound(), getPbftPeriod()};
}

PbftStep PbftManager::getPbftStep() const { return step_; }

void PbftManager::setPbftRound(PbftRound round) {
  db_->savePbftMgrField(PbftMgrField::Round, round);
  round_ = round;
}

void PbftManager::waitForPeriodFinalization() {
  do {
    // we need to be sure we finalized at least block with num lower by delegation_delay
    if (pbft_chain_->getPbftChainSize() <= final_chain_->lastBlockNumber() + final_chain_->delegationDelay()) {
      break;
    }
    thisThreadSleepForMilliSeconds(kPollingIntervalMs.count());
  } while (!stopped_);
}

std::optional<uint64_t> PbftManager::getCurrentDposTotalVotesCount() const {
  try {
    return final_chain_->dposEligibleTotalVoteCount(pbft_chain_->getPbftChainSize());
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_wr_) << "Unable to get CurrentDposTotalVotesCount for period: " << pbft_chain_->getPbftChainSize()
                 << ". Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what();
  }

  return {};
}

std::optional<uint64_t> PbftManager::getCurrentNodeVotesCount() const {
  // Note: There is a race condition in eligible_wallets_.getWalletsEligiblePeriod(). This method works only if
  // wallets eligible period == pbft chain size. This race condition is handled within pbft manager but
  // getCurrentNodeVotesCount() is called externally from standalone thread and in some edge cases we need to wait until
  // period in eligible_wallets_ is updated according to the latest chain size
  while (true) {
    if (eligible_wallets_.getWalletsEligiblePeriod() == pbft_chain_->getPbftChainSize()) {
      break;
    }

    thisThreadSleepForMilliSeconds(10);
  }

  try {
    uint64_t node_votes_count = 0;
    for (const auto &wallet : eligible_wallets_.getWallets(getPbftPeriod())) {
      // Wallet is not dpos eligible - do no vote
      if (!wallet.first) {
        continue;
      }

      node_votes_count += final_chain_->dposEligibleVoteCount(pbft_chain_->getPbftChainSize(), wallet.second.node_addr);
    }

    return node_votes_count;
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_wr_) << "Unable to get CurrentNodeVotesCount for period: " << pbft_chain_->getPbftChainSize()
                 << ". Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what();
  }

  return {};
}

void PbftManager::setPbftStep(PbftStep pbft_step) {
  db_->savePbftMgrField(PbftMgrField::Step, pbft_step);
  step_ = pbft_step;

  // Increase lambda only for odd steps (second finish steps) after node reached kMaxSteps steps
  if (step_ >= kMaxSteps && step_ % 2) {
    const auto kExponentialDefaultLambda = std::chrono::milliseconds(kGenesisConfig.pbft.lambda_ms);
    const auto [round, period] = getPbftRoundAndPeriod();
    const auto network_next_voting_step = vote_mgr_->getNetworkTplusOneNextVotingStep(period, round);

    // Once node starts exponentially backing off lambda, use kExponentialDefaultLambda as base number
    if (step_ == kMaxSteps) {
      current_round_lambda_ = kExponentialDefaultLambda;
    }

    // Node is still >= kMaxSteps steps behind the rest (at least 1/3) of the network - keep lambda at the standard
    // value so node can catch up with the rest of the nodes

    // To get withing 1 round with the rest of the network - node cannot start exponentially backing off its lambda
    // exactly when it is kMaxSteps behind the network as it would reach kMaxExponentialLambda lambda time before
    // catching up. If we delay triggering exponential backoff by 4 steps, node should get within 1 round with the
    // network.
    // !!! Important: This is true only for values kExponentialDefaultLambda = 1500ms and kMaxExponentialLambda = 60000
    // ms
    if (network_next_voting_step > step_ && network_next_voting_step - step_ >= kMaxSteps - 4 /* hardcoded delay */) {
      // Reset it only if it was already increased compared to default value
      if (current_round_lambda_ != kExponentialDefaultLambda) {
        current_round_lambda_ = kExponentialDefaultLambda;
        LOG(log_nf_) << "Node is " << network_next_voting_step - step_
                     << " steps behind the rest of the network. Reset lambda to the default value "
                     << current_round_lambda_.count() << " [ms]";
      }
    } else if (current_round_lambda_ < kMaxExponentialLambda) {
      // Node is < kMaxSteps steps behind the rest (at least 1/3) of the network - start exponentially backing off
      // lambda until it reaches kMaxExponentialLambda
      // Note: We calculate the lambda for a step independently of prior steps in case missed earlier steps.
      current_round_lambda_ *= 2;
      if (current_round_lambda_ > kMaxExponentialLambda) {
        current_round_lambda_ = kMaxExponentialLambda;
      }

      LOG(log_nf_) << "No round progress - exponentially backing off lambda to " << current_round_lambda_.count()
                   << " [ms] in step " << step_;
    }
  }
}

bool PbftManager::tryPushCertVotesBlock() {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();

  auto cert_votes = vote_mgr_->getTwoTPlusOneVotedBlockVotes(current_pbft_period, current_pbft_round,
                                                             TwoTPlusOneVotedBlockType::CertVotedBlock);
  if (cert_votes.empty()) {
    return false;
  }
  const blk_hash_t &certified_block_hash = cert_votes[0]->getBlockHash();

  LOG(log_nf_) << "Found enough cert votes for PBFT block " << certified_block_hash << ", period "
               << current_pbft_period << ", round " << current_pbft_round;

  auto pbft_block = getValidPbftProposedBlock(proposed_blocks_, current_pbft_period, certified_block_hash);
  if (!pbft_block) {
    LOG(log_er_) << "Invalid certified block " << certified_block_hash;
    return false;
  }

  // Push pbft block into chain
  if (!pushCertVotedPbftBlockIntoChain_(pbft_block, std::move(cert_votes))) {
    return false;
  }

  return true;
}

bool PbftManager::advancePeriod() {
  resetPbftConsensus(1 /* round */);
  broadcast_reward_votes_counter_ = 1;
  rebroadcast_reward_votes_counter_ = 1;
  current_period_start_datetime_ = std::chrono::system_clock::now();

  const auto chain_size = pbft_chain_->getPbftChainSize();
  const auto new_period = chain_size + 1;

  // Update wallets eligibility, call after resetPbftConsensus (waitForPeriodFinalization)
  eligible_wallets_.updateWalletsEligibility(chain_size, final_chain_);

  // Cleanup previous period votes in vote manager
  // !!!Important: we need previous period votes to get reward votes for current period block
  vote_mgr_->cleanupVotesByPeriod(chain_size);

  // Cleanup proposed blocks
  proposed_blocks_.cleanupProposedPbftBlocksByPeriod(new_period);

  LOG(log_nf_) << "Period advanced to: " << new_period << ", round and step reset to 1";

  // Restart while loop...
  return true;
}

bool PbftManager::advanceRound() {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();

  const auto new_round = vote_mgr_->determineNewRound(current_pbft_period, current_pbft_round);
  if (!new_round.has_value()) {
    return false;
  }
  assert(new_round > current_pbft_round);

  // Reset consensus
  resetPbftConsensus(*new_round);

  LOG(log_nf_) << "Round advanced to: " << *new_round << ", period " << current_pbft_period << ", step " << step_;

  // Restart while loop...
  return true;
}

void PbftManager::resetPbftConsensus(PbftRound round) {
  // Print node's broadcasted votes for current round
  printVotingSummary();

  // Cleanup saved broadcasted votes for current round
  current_round_broadcasted_votes_.clear();

  // Reset broadcast counters
  broadcast_votes_counter_ = 1;
  rebroadcast_votes_counter_ = 1;

  // Update current round and reset step to 1
  round_ = round;
  step_ = 1;
  state_ = value_proposal_state;

  const auto period = getPbftPeriod();
  uint32_t new_current_round_lambda{0};
  auto batch = db_->createWriteBatch();

  if (kGenesisConfig.state.hardforks.isOnCactiHardfork(period)) {
    // Use dynamic lambda for round 1
    if (round == 1) {
      new_current_round_lambda = dynamic_lambda_;
    } else {  // otherwise use default lambda
      new_current_round_lambda = kGenesisConfig.state.hardforks.cacti_hf.lambda_default;
    }

    // Save period lambda to db in case it's value changed or for the first block on cacti hf
    if (new_current_round_lambda != static_cast<uint32_t>(current_round_lambda_.count()) ||
        period == kGenesisConfig.state.hardforks.cacti_hf.block_num) {
      db_->savePeriodLambda(period, new_current_round_lambda, batch);
    }
  } else {
    new_current_round_lambda = kGenesisConfig.pbft.lambda_ms;
  }

  LOG(log_nf_) << "Reset PBFT consensus to: period " << period << ", round " << round << ", step 1, lambda "
               << new_current_round_lambda << " [ms]";

  current_round_lambda_ = std::chrono::milliseconds(new_current_round_lambda);

  // Update in DB first
  db_->addPbftMgrFieldToBatch(PbftMgrField::Round, round, batch);
  db_->addPbftMgrFieldToBatch(PbftMgrField::Step, 1, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);

  // Reset cert voted block in the new upcoming round
  if (cert_voted_block_for_round_.has_value()) {
    db_->removeCertVotedBlockInRound(batch);
    cert_voted_block_for_round_.reset();
  }

  // Clear all own votes generated in previous round
  vote_mgr_->clearOwnVerifiedVotes(batch);

  db_->commitWriteBatch(batch);

  // reset next voted value since start a new round
  // these are used to prevent voting multiple times while polling through the step
  // under current implementation.
  already_next_voted_null_block_hash_ = false;
  already_next_voted_value_ = false;

  if (executed_pbft_block_) {
    waitForPeriodFinalization();
    db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
    executed_pbft_block_ = false;
  }

  // Set current period & round in vote manager
  vote_mgr_->setCurrentPbftPeriodAndRound(period, round);

  current_round_start_datetime_ = std::chrono::system_clock::now();
}

void PbftManager::adjustDynamicLambda(PbftPeriod finalized_period, PbftRound finalized_round) {
  const auto &kCactiHfCfg = kGenesisConfig.state.hardforks.cacti_hf;
  rounds_count_dynamic_lambda_ += finalized_round;

  // Decrease dynamic lambda only every N blocks
  if (kCactiHfCfg.isDynamicLambdaChangeInterval(finalized_period)) {
    // If it took the same amount of rounds to finish lambda_change_interval blocks, decrease dynamic lambda
    if (rounds_count_dynamic_lambda_ == kCactiHfCfg.lambda_change_interval &&
        dynamic_lambda_ > kCactiHfCfg.lambda_min) {
      dynamic_lambda_ -= kCactiHfCfg.lambda_change;
      if (dynamic_lambda_ < kCactiHfCfg.lambda_min) {
        dynamic_lambda_ = kCactiHfCfg.lambda_min;
      }

      LOG(log_nf_) << "Decrease dynamic_lambda by " << kCactiHfCfg.lambda_change << " to " << dynamic_lambda_
                   << ", period " << finalized_period << ", round " << finalized_round;
    }

    // Reset rounds count
    rounds_count_dynamic_lambda_ = 0;
  }

  // Any time block is finalized in round > 1, increase dynamic lambda
  if (finalized_round > 1 && dynamic_lambda_ < kCactiHfCfg.lambda_max) {
    dynamic_lambda_ += kCactiHfCfg.lambda_change;
    if (dynamic_lambda_ > kCactiHfCfg.lambda_max) {
      dynamic_lambda_ = kCactiHfCfg.lambda_max;
    }

    LOG(log_nf_) << "Increase dynamic_lambda by " << kCactiHfCfg.lambda_change << " to " << dynamic_lambda_
                 << ", period " << finalized_period << ", round " << finalized_round;
  }

  auto batch = db_->createWriteBatch();
  db_->saveRoundsCountDynamicLambda(rounds_count_dynamic_lambda_, batch);
  db_->addPbftMgrFieldToBatch(PbftMgrField::Lambda, dynamic_lambda_, batch);
  db_->commitWriteBatch(batch);
}

std::chrono::milliseconds PbftManager::elapsedTimeInMs(const time_point &start_time) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time);
}

void PbftManager::sleep_() {
  // Run "wait_for" sleep in loop due to potential spurious wakeup on lock
  while (!stopped_) {
    const auto round_elapsed_time = elapsedTimeInMs(current_round_start_datetime_);
    if (next_step_time_ms_ <= round_elapsed_time) {
      return;
    }

    const auto time_to_sleep_for_ms = next_step_time_ms_ - round_elapsed_time;
    const auto [round, period] = getPbftRoundAndPeriod();
    LOG(log_tr_) << "Sleep " << time_to_sleep_for_ms.count() << " [ms] before going into the next step. Period "
                 << period << ", round " << round << ", step " << step_;
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, time_to_sleep_for_ms);
  }
}

void PbftManager::initialState() {
  // Initial PBFT state

  // Time constants...
  const auto current_pbft_period = getPbftPeriod();
  const auto current_pbft_round = db_->getPbftMgrField(PbftMgrField::Round);
  auto current_pbft_step = db_->getPbftMgrField(PbftMgrField::Step);

  // Set dynamic lambda
  const auto chain_size = current_pbft_period - 1;
  if (kGenesisConfig.state.hardforks.isOnCactiHardfork(chain_size)) {
    rounds_count_dynamic_lambda_ = db_->getRoundsCountDynamicLambda();

    if (chain_size >= 1) {
      auto dynamic_lambda = db_->getPbftMgrField(PbftMgrField::Lambda);
      // No value saved  == 1
      if (dynamic_lambda == 1) {
        LOG(log_er_) << "DB corrupted: no dynamic lambda saved for period " << chain_size;
        assert(false);
      }

      dynamic_lambda_ = dynamic_lambda;
    } else {
      dynamic_lambda_ = kGenesisConfig.state.hardforks.cacti_hf.lambda_max;
    }

    // Use dynamic lambda for round 1
    if (current_pbft_round == 1) {
      current_round_lambda_ = std::chrono::milliseconds(dynamic_lambda_);
    } else {  // otherwise use default lambda
      current_round_lambda_ = std::chrono::milliseconds(kGenesisConfig.state.hardforks.cacti_hf.lambda_default);
    }
  } else {
    current_round_lambda_ = std::chrono::milliseconds(kGenesisConfig.pbft.lambda_ms);
  }

  const auto now = std::chrono::system_clock::now();

  if (current_pbft_round == 1 && current_pbft_step == 1) {
    // Node start from scratch
    state_ = value_proposal_state;
  } else if (current_pbft_step < 4) {
    // Node start from DB, skip step 1 or 2 or 3
    current_pbft_step = 4;
    state_ = finish_state;
  } else if (current_pbft_step % 2 == 0) {
    // Node start from DB in first finishing state
    state_ = finish_state;
  } else if (current_pbft_step % 2 == 1) {
    // Node start from DB in second finishing state
    state_ = finish_polling_state;
    second_finish_step_start_datetime_ = now;
  } else {
    LOG(log_er_) << "Unexpected condition at round " << current_pbft_round << " step " << current_pbft_step;
    assert(false);
  }

  setPbftStep(current_pbft_step);
  round_ = current_pbft_round;

  // Load all proposed block from db to memory
  for (const auto &block : db_->getProposedPbftBlocks()) {
    proposed_blocks_.pushProposedPbftBlock(block, false);
  }

  // TODO[2840]: remove this check if case nodes do not log the err messages after restart
  //  if (const auto &err_msg = proposed_blocks_.checkOldBlocksPresence(current_pbft_period); err_msg.has_value()) {
  //    LOG(log_er_) << "Old proposed blocks saved in db <period> -> <blocks count>: " << *err_msg;
  //  }

  // Process saved cert voted block from db
  if (auto cert_voted_block_data = db_->getCertVotedBlockInRound(); cert_voted_block_data.has_value()) {
    const auto [cert_voted_block_round, cert_voted_block] = *cert_voted_block_data;
    if (proposed_blocks_.pushProposedPbftBlock(cert_voted_block)) {
      LOG(log_nf_) << "Last cert voted block " << cert_voted_block->getBlockHash() << " with period "
                   << cert_voted_block->getPeriod() << ", round " << cert_voted_block_round
                   << " pushed into proposed blocks";
    }

    // Set cert_voted_block_for_round_ only if round and period match. Note: could differ in edge case when node
    // crashed, new period/round was already saved in db but cert voted block was not cleared yet
    if (current_pbft_period == cert_voted_block->getPeriod() && current_pbft_round == cert_voted_block_round) {
      cert_voted_block_for_round_ = cert_voted_block;
      LOG(log_nf_) << "Init last cert voted block in round to " << cert_voted_block->getBlockHash() << ", period "
                   << current_pbft_period << ", round " << current_pbft_round;
    }
  }

  executed_pbft_block_ = db_->getPbftMgrStatus(PbftMgrStatus::ExecutedBlock);
  already_next_voted_value_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue);
  already_next_voted_null_block_hash_ = db_->getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash);

  current_round_start_datetime_ = now;
  current_period_start_datetime_ = now;
  next_step_time_ms_ = std::chrono::milliseconds(0);

  // Set current period & round in vote manager
  vote_mgr_->setCurrentPbftPeriodAndRound(current_pbft_period, current_pbft_round);

  waitForPeriodFinalization();

  const auto previous_round_next_voted_block = vote_mgr_->getTwoTPlusOneVotedBlock(
      current_pbft_period, current_pbft_round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
  const auto previous_round_next_voted_null_block = vote_mgr_->getTwoTPlusOneVotedBlock(
      current_pbft_period, current_pbft_round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock);

  LOG(log_nf_) << "Node initialize at period " << current_pbft_period << ", round " << current_pbft_round << ", step "
               << current_pbft_step << ". Previous round 2t+1 next voted null block: " << std::boolalpha
               << previous_round_next_voted_null_block.has_value() << ", previous round 2t+1 next voted block "
               << (previous_round_next_voted_block.has_value() ? previous_round_next_voted_block->abridged()
                                                               : "no value");
}

void PbftManager::setFilterState_() {
  state_ = filter_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * current_round_lambda_;
}

void PbftManager::setCertifyState_() {
  state_ = certify_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * current_round_lambda_;
  printCertStepInfo_ = true;
}

void PbftManager::setFinishState_() {
  LOG(log_dg_) << "Will go to first finish State";
  state_ = finish_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = getPbftDeadline();
}

void PbftManager::setFinishPollingState_() {
  state_ = finish_polling_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->commitWriteBatch(batch);
  already_next_voted_value_ = false;
  already_next_voted_null_block_hash_ = false;
  printSecondFinishStepInfo_ = true;
  second_finish_step_start_datetime_ = std::chrono::system_clock::now();
  next_step_time_ms_ += kPollingIntervalMs;
}

void PbftManager::loopBackFinishState_() {
  state_ = finish_state;
  setPbftStep(step_ + 1);
  auto batch = db_->createWriteBatch();
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  db_->commitWriteBatch(batch);
  already_next_voted_value_ = false;
  already_next_voted_null_block_hash_ = false;
  next_step_time_ms_ += kPollingIntervalMs;
}

void PbftManager::broadcastVotes() {
  auto net = network_.lock();
  if (!net) {
    LOG(log_er_) << "Unable to broadcast votes -> cant obtain net ptr";
    return;
  }

  // Send votes to the other peers
  auto gossipVotes = [this, &net](const std::vector<std::shared_ptr<PbftVote>> &votes,
                                  const std::string &votes_type_str, bool rebroadcast) {
    if (!votes.empty()) {
      LOG(log_dg_) << "Broadcast " << votes_type_str << " for period " << votes.back()->getPeriod() << ", round "
                   << votes.back()->getRound();
      net->gossipVotesBundle(votes, rebroadcast);
    }
  };

  // (Re)broadcast reward votes + all own pbft and pillar votes
  auto stuckPeriodBroadcastVotes = [this, &net, &gossipVotes](bool rebroadcast) {
    auto [round, period] = getPbftRoundAndPeriod();

    gossipVotes(vote_mgr_->getRewardVotes(), "Reward votes", rebroadcast);

    // Broadcast own pbft votes - send votes by one as they have different type, period, round, step
    if (const auto &own_votes = vote_mgr_->getOwnVerifiedVotes(); !own_votes.empty()) {
      for (const auto &vote : own_votes) {
        net->gossipVote(vote, getPbftProposedBlock(vote->getPeriod(), vote->getBlockHash()), rebroadcast);
      }

      LOG(log_dg_) << "Broadcast own votes for period " << period << ", round " << round << ", rebroadcast "
                   << rebroadcast;
    }

    // Broadcast own pillar vote
    if (const auto &own_pillar_vote = db_->getOwnPillarBlockVote(); own_pillar_vote) {
      net->gossipPillarBlockVote(own_pillar_vote, rebroadcast);
    }
  };

  // (Re)broadcast 2t+1 soft/reward/previous round next votes + all own votes
  auto stuckRoundBroadcastVotes = [this, &gossipVotes, &stuckPeriodBroadcastVotes](bool rebroadcast) {
    auto [round, period] = getPbftRoundAndPeriod();

    stuckPeriodBroadcastVotes(rebroadcast);

    // Broadcast 2t+1 soft votes
    gossipVotes(vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock),
                "2t+1 soft votes", rebroadcast);

    // Broadcast previous round 2t+1 next votes
    if (round > 1) {
      gossipVotes(
          vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock),
          "2t+1 next votes", rebroadcast);
      gossipVotes(
          vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock),
          "2t+1 next null votes", rebroadcast);
    }
  };

  const auto round_elapsed_time = elapsedTimeInMs(current_round_start_datetime_);
  const auto period_elapsed_time = elapsedTimeInMs(current_period_start_datetime_);

  if (round_elapsed_time / current_round_lambda_ > kRebroadcastVotesLambdaTime * rebroadcast_votes_counter_) {
    // Stalled in the same round for kRebroadcastVotesLambdaTime * current_round_lambda_ time -> rebroadcast votes
    stuckRoundBroadcastVotes(true);
    rebroadcast_votes_counter_++;
    // If there was a rebroadcast no need to do next broadcast either
    broadcast_votes_counter_++;
  } else if (round_elapsed_time / current_round_lambda_ > kBroadcastVotesLambdaTime * broadcast_votes_counter_) {
    // Stalled in the same round for kBroadcastVotesLambdaTime * current_round_lambda_ time -> broadcast votes
    stuckRoundBroadcastVotes(false);
    broadcast_votes_counter_++;
  } else if (period_elapsed_time / current_round_lambda_ >
             kRebroadcastVotesLambdaTime * rebroadcast_reward_votes_counter_) {
    // Stalled in the same period for kRebroadcastVotesLambdaTime * current_round_lambda_ time -> rebroadcast reward
    // votes
    stuckPeriodBroadcastVotes(true);
    rebroadcast_reward_votes_counter_++;
    // If there was a rebroadcast no need to do next broadcast either
    broadcast_reward_votes_counter_++;
  } else if (period_elapsed_time / current_round_lambda_ >
             kBroadcastVotesLambdaTime * broadcast_reward_votes_counter_) {
    // Stalled in the same period for kBroadcastVotesLambdaTime * current_round_lambda_ time -> broadcast reward votes
    stuckPeriodBroadcastVotes(false);
    broadcast_reward_votes_counter_++;
  }
}

void PbftManager::testBroadcastVotesFunctionality() {
  // Set these variables to force broadcastVotes() send votes
  current_round_start_datetime_ = time_point{};
  current_period_start_datetime_ = time_point{};

  broadcastVotes();
}

void PbftManager::printVotingSummary() const {
  const auto [round, period] = getPbftRoundAndPeriod();
  Json::Value json_obj;

  json_obj["period"] = Json::UInt64(period - 1);
  json_obj["round"] = Json::UInt64(round);
  auto &steps_voted_blocks_json = json_obj["voted_blocks_steps"] = Json::Value(Json::arrayValue);

  for (const auto &voted_blocks_steps : current_round_broadcasted_votes_) {
    const auto voted_block_hash = voted_blocks_steps.first;
    auto &voted_blocks_steps_json = steps_voted_blocks_json.append(Json::Value(Json::objectValue));
    auto &steps_json = voted_blocks_steps_json[voted_block_hash.abridged().substr(0, 8)] =
        Json::Value(Json::arrayValue);
    for (const auto &step : voted_blocks_steps.second) {
      steps_json.append(step);
    }
  }

  LOG(log_nf_) << "Voting summary: " << jsonToUnstyledString(json_obj);
}

bool PbftManager::stateOperations_() {
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_tr_) << "PBFT current period: " << period << ", round: " << round << ", step " << step_;

  // Process synced blocks
  pushSyncedPbftBlocksIntoChain();

  auto net = network_.lock();
  // Only broadcast votes and try to push cert voted block if node is not syncing
  if (net && !net->pbft_syncing()) {
    // (Re)broadcast votes if needed
    broadcastVotes();

    // Check if there is 2t+1 cert votes for some valid block, if so - push it into the chain
    if (tryPushCertVotesBlock()) {
      return true;
    }
  }

  // Check if there is 2t+1 next votes for some valid block, if so - advance round
  if (advanceRound()) {
    return true;
  }

  // Note: do not use local variable period as it might be outdated, always use getPbftPeriod()
  const auto &wallets = eligible_wallets_.getWallets(getPbftPeriod());
  if (std::any_of(wallets.cbegin(), wallets.cend(), [](const auto &wallet) {
        return wallet.first; /* is dpos eligible */
      })) {
    return false;
  }

  // If node is not eligible to vote and create block, always return true so pbft state machine never enters specific
  // consensus steps (propose, soft-vote, cert-vote, next-vote). Nodes that have no delegation should just
  // observe 2t+1 cert votes to move to the next period or 2t+1 next votes to move to the next round

  // Check 2t+1 cert/next votes every kPollingIntervalMs
  std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
  return true;
}

std::shared_ptr<PbftBlock> PbftManager::getValidPbftProposedBlock(ProposedBlocks &proposed_blocks, PbftPeriod period,
                                                                  const blk_hash_t &block_hash) {
  const auto block_data = proposed_blocks.getPbftProposedBlock(period, block_hash);
  if (!block_data.has_value()) {
    LOG(log_er_) << "Unable to find proposed block " << block_hash << ", period " << period;
    return nullptr;
  }

  const auto block = block_data->first;
  assert(block != nullptr);

  // Block is not validated yet
  if (!block_data->second) {
    if (!validatePbftBlock(block)) {
      LOG(log_er_) << "Proposed block " << block_hash << " failed validation, period " << period;
      return nullptr;
    }

    proposed_blocks.markBlockAsValid(block);
  }

  return block;
}

bool PbftManager::genAndPlaceVote(PbftVoteTypes vote_type, PbftPeriod period, PbftRound round, PbftStep step,
                                  const blk_hash_t &block_hash, std::shared_ptr<PbftBlock> pbft_block) {
  if (pbft_block) {
    assert(pbft_block->getPeriod() == period);
    assert(pbft_block->getBlockHash() == block_hash);
  }

  // In case it is pbft with pillar block period and we have not voted yet, place a pillar vote (can be placed during
  // any pbft step)
  std::optional<blk_hash_t> place_pillar_vote_for_block;
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(period) &&
      last_placed_pillar_vote_period_ < period) {
    if (pbft_block) {
      // No need to check presence of extra data and pillar block hash - this was already validated in validatePbftBlock
      place_pillar_vote_for_block = pbft_block->getExtraData()->getPillarBlockHash();
    } else {
      const auto current_pillar_block = pillar_chain_mgr_->getCurrentPillarBlock();
      // Check if the latest pillar block was created
      if (current_pillar_block && current_pillar_block->getPeriod() == period - 1) {
        place_pillar_vote_for_block = current_pillar_block->getHash();
      }
    }
  }

  bool success = false;
  std::vector<std::shared_ptr<PbftVote>> valid_votes;
  uint64_t valid_votes_weight = 0;
  for (const auto &wallet : eligible_wallets_.getWallets(period)) {
    // Wallet is not dpos eligible - do no vote
    if (!wallet.first) {
      continue;
    }

    const auto vote = vote_mgr_->generateVoteWithWeight(block_hash, vote_type, period, round, step, wallet.second);
    if (!vote) {
      LOG(log_dg_) << "Failed to generate vote for " << block_hash << ", period " << period << ", round " << round
                   << ", step " << step << ", validator " << wallet.second.node_addr;
      continue;
    }

    if (!vote_mgr_->addVerifiedVote(vote)) {
      LOG(log_er_) << "Unable to place vote " << vote->getHash() << " for block " << block_hash << ", period " << period
                   << ", round " << round << ", step " << step << ", validator " << wallet.second.node_addr;
      continue;
    }

    // Propose votes are sent as single packets so it is gossiped together with pbft block
    if (vote_type == PbftVoteTypes::propose_vote) {
      gossipNewOwnVote(vote, pbft_block);

      LOG(log_nf_) << "Placed and sent " << vote->getHash() << " vote for block " << block_hash << ", vote weight "
                   << *vote->getWeight() << ", period " << period << ", round " << round << ", step " << step
                   << ", validator " << wallet.second.node_addr;
    } else {
      valid_votes_weight += *vote->getWeight();
      valid_votes.push_back(std::move(vote));

      LOG(log_nf_) << "Placed " << vote->getHash() << " vote for block " << block_hash << ", vote weight "
                   << *vote->getWeight() << ", period " << period << ", round " << round << ", step " << step
                   << ", validator " << wallet.second.node_addr;
    }

    // Save own verified vote
    vote_mgr_->saveOwnVerifiedVote(vote);

    if (place_pillar_vote_for_block.has_value()) {
      const auto pillar_vote = pillar_chain_mgr_->genAndPlacePillarVote(period, *place_pillar_vote_for_block,
                                                                        wallet.second.node_secret, true);
      if (pillar_vote) {
        last_placed_pillar_vote_period_ = pillar_vote->getPeriod();
      }
    }
    success = true;
  }

  // Gossip all generated votes in single packet
  if (!valid_votes.empty()) {
    if (valid_votes.size() == 1) {
      const auto &vote = valid_votes.front();
      gossipNewOwnVote(vote, pbft_block);
      LOG(log_nf_) << "Sent " << vote->getHash() << " vote for block " << block_hash << ", vote weight "
                   << *vote->getWeight() << ", period " << period << ", round " << round << ", step " << step;
    } else {
      gossipNewOwnVotesBundle(valid_votes);
      LOG(log_nf_) << "Votes bundle with " << valid_votes.size() << " votes with overall weight " << valid_votes_weight
                   << " for block " << block_hash << ", period " << period << ", round " << round << ", step " << step
                   << " sent";
    }
  }

  return success;
}

bool PbftManager::genAndPlaceProposeVote(const std::shared_ptr<PbftBlock> &proposed_block,
                                         std::vector<std::shared_ptr<PbftVote>> &&reward_votes) {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();
  const auto current_pbft_step = getPbftStep();

  if (proposed_block->getPeriod() != current_pbft_period) {
    LOG(log_er_) << "Propose block " << proposed_block->getBlockHash()
                 << " has different period than current pbft period " << current_pbft_period;
    return false;
  }

  // Broadcast reward votes - previous round 2t+1 cert votes
  if (auto net = network_.lock()) {
    LOG(log_dg_) << "Broadcast propose block reward votes for block " << proposed_block->getBlockHash()
                 << ", num of reward votes: " << reward_votes.size() << ", period " << current_pbft_period << ", round "
                 << current_pbft_round;
    net->gossipVotesBundle(reward_votes, false);
  }

  if (!genAndPlaceVote(PbftVoteTypes::propose_vote, current_pbft_period, current_pbft_round, current_pbft_step,
                       proposed_block->getBlockHash(), proposed_block)) {
    LOG(log_nf_) << "Unable to generate and place propose vote";
    return false;
  }

  return true;
}

void PbftManager::gossipNewOwnVote(const std::shared_ptr<PbftVote> &vote,
                                   const std::shared_ptr<PbftBlock> &voted_block) {
  // Always rebroadcast pbft block together with new own propose vote
  bool rebroadcast = (vote->getType() == PbftVoteTypes::propose_vote);
  gossipVote(vote, voted_block, rebroadcast);

  auto found_voted_block_it = current_round_broadcasted_votes_.find(vote->getBlockHash());
  if (found_voted_block_it == current_round_broadcasted_votes_.end()) {
    found_voted_block_it = current_round_broadcasted_votes_.insert({vote->getBlockHash(), {}}).first;
  }

  found_voted_block_it->second.emplace_back(vote->getStep());
}

void PbftManager::gossipNewOwnVotesBundle(const std::vector<std::shared_ptr<PbftVote>> &votes) {
  auto net = network_.lock();
  if (!net) {
    LOG(log_er_) << "Could not obtain net - cannot gossip new own votes bundle";
    // assert(false);
    return;
  }

  net->gossipVotesBundle(votes);

  for (const auto &vote : votes) {
    auto found_voted_block_it = current_round_broadcasted_votes_.find(vote->getBlockHash());
    if (found_voted_block_it == current_round_broadcasted_votes_.end()) {
      found_voted_block_it = current_round_broadcasted_votes_.insert({vote->getBlockHash(), {}}).first;
    }

    found_voted_block_it->second.emplace_back(vote->getStep());
  }
}

void PbftManager::gossipVote(const std::shared_ptr<PbftVote> &vote, const std::shared_ptr<PbftBlock> &voted_block,
                             bool rebroadcast) {
  assert(!voted_block || vote->getBlockHash() == voted_block->getBlockHash());

  auto net = network_.lock();
  if (!net) {
    LOG(log_er_) << "Could not obtain net - cannot gossip new own vote";
    // assert(false);
    return;
  }

  net->gossipVote(vote, voted_block, rebroadcast);
}

void PbftManager::proposeBlock_() {
  // Value Proposal
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT value proposal state in period " << period << ", round " << round;

  if (round == 1 ||
      vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock)
          .has_value()) {
    LOG(log_nf_) << " 2t+1 next voted kNullBlockHash in previous round " << round - 1;

    // Propose new block
    if (auto proposed_block_data = proposePbftBlock(); proposed_block_data.has_value()) {
      // Broadcast reward votes - previous round 2t+1 cert votes
      if (auto net = network_.lock()) {
        LOG(log_dg_) << "Broadcast propose block reward votes for block "
                     << proposed_block_data->pbft_block->getBlockHash()
                     << ", num of reward votes: " << proposed_block_data->reward_votes.size() << ", period " << period
                     << ", round " << round;
        net->gossipVotesBundle(proposed_block_data->reward_votes, false);
      }

      // Broadcast new propose vote + proposed block
      gossipNewOwnVote(proposed_block_data->vote, proposed_block_data->pbft_block);

      LOG(log_nf_) << "Placed " << proposed_block_data->vote->getHash() << " propose vote for block "
                   << proposed_block_data->pbft_block->getBlockHash() << ", vote weight "
                   << *proposed_block_data->vote->getWeight() << ", period " << period << ", round " << round
                   << ", step " << step_ << ", validator " << proposed_block_data->vote->getVoterAddr();
    }

    return;
  } else if (const auto previous_round_next_voted_value =
                 vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
             previous_round_next_voted_value.has_value()) {
    // previous_round_next_voted_value_ should never have value for round == 1
    assert(round > 1);

    // Round greater than 1 and next voted some value that is not null block hash
    const auto &next_voted_block_hash = *previous_round_next_voted_value;

    const auto next_voted_block = getValidPbftProposedBlock(proposed_blocks_, period, next_voted_block_hash);
    if (!next_voted_block) {
      // This should never happen - if so, we probably have a bug in storing the blocks in proposed_blocks_
      LOG(log_er_) << "Unable to re-propose previous round next voted block " << next_voted_block_hash << ", period "
                   << period << ", round " << round;
      return;
    }

    auto block_reward_votes = vote_mgr_->checkRewardVotes(next_voted_block, true);
    if (!block_reward_votes.first) {
      LOG(log_er_) << "Unable to re-propose previous round next voted block " << next_voted_block_hash << ", period "
                   << period << ", round " << round << ". Reward votes for this block were not found";
      return;
    }

    genAndPlaceProposeVote(next_voted_block, std::move(block_reward_votes.second));
    return;
  } else {
    LOG(log_er_) << "Previous round " << round - 1 << " doesn't have enough next votes, period " << period;
    assert(false);
  }
}

void PbftManager::identifyBlock_() {
  // The Filtering Step
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT filtering state in period: " << period << ", round: " << round;

  if (round == 1 ||
      vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock)
          .has_value()) {
    // Identity leader
    const auto leader_block_data = identifyLeaderBlock(proposed_blocks_, vote_mgr_->getProposalVotes(period, round));
    if (!leader_block_data.has_value()) {
      LOG(log_dg_) << "No leader block identified. Period " << period << ", round " << round;
      return;
    }

    assert(leader_block_data->first->getPeriod() == period);
    LOG(log_dg_) << "Leader block identified " << leader_block_data->first->getBlockHash() << ", period " << period
                 << ", round " << round;

    genAndPlaceVote(PbftVoteTypes::soft_vote, leader_block_data->first->getPeriod(), round, step_,
                    leader_block_data->first->getBlockHash(), leader_block_data->first);
  } else if (const auto previous_round_next_voted_value =
                 vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
             previous_round_next_voted_value.has_value()) {
    const auto &next_voted_block_hash = *previous_round_next_voted_value;
    const auto next_voted_block = getValidPbftProposedBlock(proposed_blocks_, period, next_voted_block_hash);
    if (!next_voted_block) {
      // This should never happen - if so, we probably have a bug in storing the blocks in proposed_blocks_
      LOG(log_er_) << "Unable to soft-vote previous round next voted block " << next_voted_block_hash << ", period "
                   << period << ", round " << round;
      return;
    }

    genAndPlaceVote(PbftVoteTypes::soft_vote, period, round, step_, next_voted_block_hash, next_voted_block);
  }
}

void PbftManager::certifyBlock_() {
  // The Certifying Step
  auto [round, period] = getPbftRoundAndPeriod();

  if (printCertStepInfo_) {
    LOG(log_dg_) << "PBFT certifying state in period " << period << ", round " << round;
    printCertStepInfo_ = false;
  }

  const auto elapsed_time_in_round = elapsedTimeInMs(current_round_start_datetime_);
  go_finish_state_ = elapsed_time_in_round > getPbftDeadline() - kPollingIntervalMs;
  if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in period " << period << ", round " << round;

    uint64_t votes_weight = 0;
    std::string debug_msg;
    auto soft_votes = vote_mgr_->getStepVotes(period, round, 2 /* soft voting step */);
    for (const auto &block_soft_votes : soft_votes.votes) {
      votes_weight += block_soft_votes.second.weight;
      debug_msg += "Block " + block_soft_votes.first.abridged() + "(votes weight " +
                   std::to_string(block_soft_votes.second.weight) + ") -> [";

      for (const auto &vote : block_soft_votes.second.votes) {
        debug_msg += vote.first.abridged() + "(voter " + vote.second->getVoterAddr().abridged() + "), ";
      }

      debug_msg += "]\n";
    }
    debug_msg += "all votes weight " + std::to_string(votes_weight) + ", 2t+1 threshold " +
                 std::to_string(vote_mgr_->getPbftTwoTPlusOne(period - 1, PbftVoteTypes::soft_vote).value());
    LOG(log_dg_) << debug_msg;

    return;
  }

  // Should not happen, add log here for safety checking
  if (elapsed_time_in_round < 2 * current_round_lambda_) {
    LOG(log_er_) << "PBFT Reached step 3 too quickly after only " << elapsed_time_in_round.count() << " [ms] in period "
                 << period << ", round " << round;
    return;
  }

  // Already sent cert voted in this round
  if (cert_voted_block_for_round_.has_value()) {
    LOG(log_dg_) << "Already did cert vote in period " << period << ", round " << round;
    return;
  }

  // Get 2t+1 soft voted bock hash
  const auto soft_voted_block_hash =
      vote_mgr_->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock);
  if (!soft_voted_block_hash.has_value()) {
    LOG(log_dg_) << "Certify: Not enough soft votes for current round yet. Period " << period << ",  round " << round;
    return;
  }

  // Get 2t+1 soft voted bock
  const auto soft_voted_block = getValidPbftProposedBlock(proposed_blocks_, period, *soft_voted_block_hash);
  if (soft_voted_block == nullptr) {
    LOG(log_dg_) << "Certify: invalid 2t+1 soft voted block " << *soft_voted_block_hash << ". Period " << period
                 << ",  round " << round;
    return;
  }

  // generate cert vote
  if (!genAndPlaceVote(PbftVoteTypes::cert_vote, soft_voted_block->getPeriod(), round, step_,
                       soft_voted_block->getBlockHash(), soft_voted_block)) {
    LOG(log_dg_) << "Failed to generate and place cert vote for " << soft_voted_block->getBlockHash();
    return;
  }

  cert_voted_block_for_round_ = soft_voted_block;
  db_->saveCertVotedBlockInRound(round, soft_voted_block);
}

void PbftManager::firstFinish_() {
  // Even number steps from 4 are in first finish
  auto [round, period] = getPbftRoundAndPeriod();
  LOG(log_dg_) << "PBFT first finishing state in period " << period << ", round " << round << ", step " << step_;

  if (cert_voted_block_for_round_.has_value()) {
    const auto &cert_voted_block = *cert_voted_block_for_round_;

    // It should never happen that node moved to the next period without cert_voted_block_for_round_ reset
    assert(cert_voted_block->getPeriod() == period);

    genAndPlaceVote(PbftVoteTypes::next_vote, cert_voted_block->getPeriod(), round, step_,
                    cert_voted_block->getBlockHash(), cert_voted_block);
  } else if (round >= 2 &&
             vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock)
                 .has_value()) {
    // Starting value in round 1 is always null block hash... So combined with other condition for next
    // voting null block hash...
    genAndPlaceVote(PbftVoteTypes::next_vote, period, round, step_, kNullBlockHash);
  } else {
    // TODO: We should vote for any value that we first saw 2t+1 next votes for in previous round -> in current design
    // we dont know for which value we saw 2t+1 next votes as first so we prefer specific block if possible
    std::pair<blk_hash_t, std::shared_ptr<PbftBlock>> starting_value;

    const auto previous_round_next_voted_value =
        vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
    if (previous_round_next_voted_value.has_value()) {
      auto block = getValidPbftProposedBlock(proposed_blocks_, period, *previous_round_next_voted_value);
      if (!block) {
        // This should never happen - if so, we probably have a bug in storing the blocks in proposed_blocks_
        LOG(log_er_) << "Unable to first finish next-vote starting value " << *previous_round_next_voted_value
                     << ". Period " << period << ", round " << round;
        return;
      }

      starting_value = {*previous_round_next_voted_value, std::move(block)};
    } else {  // for round == 1, starting value is always kNullBlockHash and previous round next voted null_block_has_
              // should be == false
      // This should never happen as round >= 2 && previous_round_next_voted_block == kNullBlockHash is covered in
      // previous "else if" condition
      assert(!vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock)
                  .has_value());
      starting_value = {kNullBlockHash, nullptr};
    }

    genAndPlaceVote(PbftVoteTypes::next_vote, period, round, step_, starting_value.first, starting_value.second);
  }
}

void PbftManager::secondFinish_() {
  // Odd number steps from 5 are in second finish
  auto [round, period] = getPbftRoundAndPeriod();

  if (printSecondFinishStepInfo_) {
    LOG(log_dg_) << "PBFT second finishing state in period " << period << ", round " << round << ", step " << step_;
    printSecondFinishStepInfo_ = false;
  }

  // Lambda function for next voting 2t+1 soft voted block from current round
  auto next_vote_soft_voted_block = [this, period = period, round = round]() {
    if (already_next_voted_value_) {
      return;
    }

    // Get 2t+1 soft voted bock hash
    const auto soft_voted_block_hash =
        vote_mgr_->getTwoTPlusOneVotedBlock(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock);
    if (!soft_voted_block_hash.has_value()) {
      LOG(log_tr_) << "Second finish: Not enough soft votes for current round yet. Period " << period << ",  round "
                   << round;
      return;
    }

    // Get 2t+1 soft voted bock
    const auto soft_voted_block = getValidPbftProposedBlock(proposed_blocks_, period, *soft_voted_block_hash);
    if (soft_voted_block == nullptr) {
      LOG(log_dg_) << "Second finish: invalid 2t+1 soft voted block " << *soft_voted_block_hash << ". Period " << period
                   << ",  round " << round;
      return;
    }

    if (genAndPlaceVote(PbftVoteTypes::next_vote, period, round, step_, *soft_voted_block_hash, soft_voted_block)) {
      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      already_next_voted_value_ = true;
    }
  };

  // Try to next vote 2t+1 soft voted block from current round
  next_vote_soft_voted_block();

  // Lambda function for next voting 2t+1 next voted null block from previous round
  auto next_vote_null_block = [this, period = period, round = round]() {
    if (cert_voted_block_for_round_.has_value() || already_next_voted_null_block_hash_ || round < 2) {
      return;
    }

    // Get 2t+1 next voted null bock from previous round
    const auto next_voted_null_block_hash =
        vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock);
    if (!next_voted_null_block_hash.has_value()) {
      LOG(log_tr_) << "Second finish: Not enough null next votes from previous round. Period " << period << ",  round "
                   << round;
      return;
    }

    if (genAndPlaceVote(PbftVoteTypes::next_vote, period, round, step_, kNullBlockHash)) {
      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      already_next_voted_null_block_hash_ = true;
    }
  };

  // Try to next vote 2t+1 next voted null block from previous round
  next_vote_null_block();

  loop_back_finish_state_ =
      elapsedTimeInMs(second_finish_step_start_datetime_) > 2 * (current_round_lambda_ - kPollingIntervalMs);
}

std::optional<PbftManager::ProposedBlockData> PbftManager::generatePbftBlock(
    PbftPeriod propose_period, const blk_hash_t &prev_blk_hash, const blk_hash_t &anchor_hash,
    const blk_hash_t &order_hash, const std::optional<PbftBlockExtraData> &extra_data,
    const std::vector<WalletConfig> &eligible_wallets) {
  // Reward votes should only include those reward votes with the same round as the round last pbft block was pushed
  // into chain
  auto reward_votes = vote_mgr_->getRewardVotes();
  if (propose_period > 1) [[likely]] {
    assert(!reward_votes.empty());
    if (reward_votes[0]->getPeriod() != propose_period - 1) {
      LOG(log_er_) << "Reward vote period(" << reward_votes[0]->getPeriod() << ") != propose_period - 1("
                   << propose_period - 1 << ")";
      assert(false);
      return {};
    }
  }

  std::vector<vote_hash_t> reward_votes_hashes;
  std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                 [](const auto &v) { return v->getHash(); });

  auto final_chain_hash = final_chain_->finalChainHash(propose_period);
  if (!final_chain_hash) {
    LOG(log_wr_) << "Block for period " << propose_period << " could not be proposed as we are behind";
    return {};
  }
  try {
    ProposedBlocks propose_blocks{nullptr};
    std::vector<std::shared_ptr<PbftVote>> propose_votes;

    for (const auto &wallet : eligible_wallets) {
      auto block =
          std::make_shared<PbftBlock>(prev_blk_hash, anchor_hash, order_hash, final_chain_hash.value(), propose_period,
                                      wallet.node_addr, wallet.node_secret, reward_votes_hashes, extra_data);

      auto propose_vote = vote_mgr_->generateVoteWithWeight(block->getBlockHash(), PbftVoteTypes::propose_vote,
                                                            propose_period, round_, step_, wallet);
      if (!propose_vote) {
        LOG(log_er_) << "Failed to generate propose vote for block " << block->getBlockHash() << ", period "
                     << propose_period << ", round " << round_ << ", step " << step_ << ", validator "
                     << wallet.node_addr << " when generating pbft block";
        continue;
      }

      if (!vote_mgr_->isUniqueVote(propose_vote).first) {
        LOG(log_er_) << "Non unique propose vote " << propose_vote->getHash() << " for block " << block->getBlockHash()
                     << ", period " << propose_period << ", round " << propose_vote->getRound() << ", step "
                     << propose_vote->getStep() << ", validator " << wallet.node_addr;
        continue;
      }

      propose_blocks.pushProposedPbftBlock(block, false);
      propose_votes.push_back(std::move(propose_vote));
    }

    // Select leader block
    auto leader_block_data = identifyLeaderBlock(propose_blocks, std::move(propose_votes));
    if (!leader_block_data.has_value()) {
      return {};
    }

    if (!vote_mgr_->addVerifiedVote(leader_block_data->second)) {
      LOG(log_er_) << "Unable to save propose vote " << leader_block_data->second->getHash() << " for block "
                   << leader_block_data->second->getBlockHash() << ", period " << propose_period << ", round "
                   << leader_block_data->second->getRound() << ", step " << leader_block_data->second->getStep()
                   << ", validator " << leader_block_data->second->getVoterAddr();
      return {};
    }

    // Save own verified vote
    proposed_blocks_.pushProposedPbftBlock(leader_block_data->first);
    vote_mgr_->saveOwnVerifiedVote(leader_block_data->second);

    return PbftManager::ProposedBlockData{std::move(leader_block_data->first), std::move(reward_votes),
                                          std::move(leader_block_data->second)};
  } catch (const std::exception &e) {
    LOG(log_er_) << "Block for period " << propose_period << " could not be proposed " << e.what();
    return {};
  }
}

void PbftManager::processProposedBlock(const std::shared_ptr<PbftBlock> &proposed_block) {
  if (proposed_blocks_.isInProposedBlocks(proposed_block->getPeriod(), proposed_block->getBlockHash())) {
    return;
  }

  proposed_blocks_.pushProposedPbftBlock(proposed_block);
}

blk_hash_t PbftManager::calculateOrderHash(const std::vector<blk_hash_t> &dag_block_hashes) {
  if (dag_block_hashes.empty()) {
    return kNullBlockHash;
  }
  dev::RLPStream order_stream(1);
  order_stream.appendList(dag_block_hashes.size());
  for (auto const &blk_hash : dag_block_hashes) {
    order_stream << blk_hash;
  }
  return dev::sha3(order_stream.out());
}

blk_hash_t PbftManager::calculateOrderHash(const std::vector<std::shared_ptr<DagBlock>> &dag_blocks) {
  if (dag_blocks.empty()) {
    return kNullBlockHash;
  }
  dev::RLPStream order_stream(1);
  order_stream.appendList(dag_blocks.size());
  for (auto const &blk : dag_blocks) {
    order_stream << blk->getHash();
  }
  return dev::sha3(order_stream.out());
}

std::optional<blk_hash_t> findClosestAnchor(const std::vector<blk_hash_t> &ghost,
                                            const std::vector<blk_hash_t> &dag_order, uint32_t included) {
  for (uint32_t i = included; i > 0; i--) {
    if (std::count(ghost.begin(), ghost.end(), dag_order[i - 1])) {
      return dag_order[i - 1];
    }
  }
  return ghost[1];
}

struct ProposedBlockData {
  std::shared_ptr<PbftBlock> pbft_block;
  std::vector<std::shared_ptr<PbftVote>> reward_votes;
  WalletConfig proposer_wallet;
};

std::optional<PbftManager::ProposedBlockData> PbftManager::proposePbftBlock() {
  // generates propose vote with the same block
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();

  // List of wallets that are eligible to propose pbft block during current period
  std::vector<WalletConfig> eligible_wallets;
  for (const auto &wallet : eligible_wallets_.getWallets(current_pbft_period)) {
    // Wallet is not dpos eligible - do no try to propose block
    if (!wallet.first) {
      continue;
    }

    if (!vote_mgr_->genAndValidateVrfSortition(current_pbft_period, current_pbft_round, wallet.second)) {
      LOG(log_dg_) << "Unable to propose block for period " << current_pbft_period << ", round " << current_pbft_round
                   << ", validator " << wallet.second.node_addr << ". Invalid vrf sortition";
      continue;
    }

    eligible_wallets.push_back(wallet.second);
  }

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  auto last_period_dag_anchor_block_hash = pbft_chain_->getLastNonNullPbftBlockAnchor();
  if (last_period_dag_anchor_block_hash == kNullBlockHash) {
    last_period_dag_anchor_block_hash = dag_genesis_block_hash_;
  }

  // Creates pbft block's extra data
  std::optional<PbftBlockExtraData> extra_data;
  if (kGenesisConfig.state.hardforks.ficus_hf.isFicusHardfork(current_pbft_period)) {
    extra_data = createPbftBlockExtraData(current_pbft_period);
    if (!extra_data.has_value()) {
      LOG(log_er_) << "Unable to propose block for period " << current_pbft_period << ", round " << current_pbft_round
                   << ". Empty extra data";
      return {};
    }
  }

  auto ghost = dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at least include the last period dag anchor block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash, extra_data,
                             eligible_wallets);
  }

  blk_hash_t dag_block_hash;
  if (ghost.size() <= kGenesisConfig.pbft.dag_blocks_size) {
    // Move back config_.ghost_path_move_back DAG blocks for DAG sycning
    auto ghost_index = (ghost.size() < kGenesisConfig.pbft.ghost_path_move_back + 1)
                           ? 0
                           : (ghost.size() - 1 - kGenesisConfig.pbft.ghost_path_move_back);
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = ghost[ghost_index];
  } else {
    dag_block_hash = ghost[kGenesisConfig.pbft.dag_blocks_size - 1];
  }

  if (dag_block_hash == dag_genesis_block_hash_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash, extra_data,
                             eligible_wallets);
  }

  // Compare with last dag block hash. If they are same, which means no new dag blocks generated since last round. In
  // that case PBFT proposer should propose NULL BLOCK HASH anchor as their value
  if (dag_block_hash == last_period_dag_anchor_block_hash) {
    auto non_finalized_dag_blocks = dag_mgr_->getNonFinalizedBlocks();
    if (non_finalized_dag_blocks.second.size() > 0) {
      dag_block_hash = *non_finalized_dag_blocks.second.rbegin()->second.begin();
    } else {
      LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                   << " No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
      LOG(log_dg_) << "Ghost: " << ghost;
      return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash, extra_data,
                               eligible_wallets);
    }
  }

  // get DAG block and transaction order
  auto dag_block_order = dag_mgr_->getDagBlockOrder(dag_block_hash, current_pbft_period);

  if (dag_block_order.empty()) {
    LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlockOrder failed in propose";
    assert(false);
  }

  u256 total_weight = 0;
  uint32_t dag_blocks_included = 0;
  for (auto const &blk_hash : dag_block_order) {
    auto dag_blk = dag_mgr_->getDagBlock(blk_hash);
    if (!dag_blk) {
      LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlock failed in propose for block "
                   << blk_hash;
      assert(false);
    }
    const auto &dag_block_weight = dag_blk->getGasEstimation();

    const auto [dag_gas_limit, pbft_gas_limit] = kGenesisConfig.getGasLimits(current_pbft_period);
    if (total_weight + dag_block_weight > pbft_gas_limit) {
      break;
    }
    total_weight += dag_block_weight;
    dag_blocks_included++;
  }

  if (dag_blocks_included != dag_block_order.size()) {
    auto closest_anchor = findClosestAnchor(ghost, dag_block_order, dag_blocks_included);
    if (!closest_anchor) {
      LOG(log_er_) << "Can't find closest anchor after block clipping. Ghost: " << ghost << ". Clipped block_order: "
                   << vec_blk_t(dag_block_order.begin(), dag_block_order.begin() + dag_blocks_included);
      assert(false);
    }

    dag_block_hash = *closest_anchor;
    dag_block_order = dag_mgr_->getDagBlockOrder(dag_block_hash, current_pbft_period);
  }
  auto order_hash = calculateOrderHash(dag_block_order);

  if (auto proposed_block_data = generatePbftBlock(current_pbft_period, last_pbft_block_hash, dag_block_hash,
                                                   order_hash, extra_data, eligible_wallets);
      proposed_block_data.has_value()) {
    LOG(log_nf_) << "Created PBFT block: " << proposed_block_data->pbft_block->getBlockHash()
                 << ", order hash:" << order_hash << ", DAG order " << dag_block_order;
    return proposed_block_data;
  }

  return {};
}

std::optional<PbftBlockExtraData> PbftManager::createPbftBlockExtraData(PbftPeriod pbft_period) const {
  std::optional<blk_hash_t> pillar_block_hash;
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(pbft_period)) {
    // Anchor pillar block hash into the pbft block
    const auto pillar_block = pillar_chain_mgr_->getCurrentPillarBlock();
    if (!pillar_block) {
      LOG(log_er_) << "Missing pillar block, pbft period " << pbft_period;
      return {};
    }

    if (pillar_block->getPeriod() != pbft_period - 1) {
      LOG(log_er_) << "Wrong pillar block period: " << pillar_block->getPeriod() << ", pbft period: " << pbft_period;
      return {};
    }

    pillar_block_hash = pillar_block->getHash();
  }

  return PbftBlockExtraData{TARAXA_MAJOR_VERSION, TARAXA_MINOR_VERSION, TARAXA_PATCH_VERSION, TARAXA_NET_VERSION, "T",
                            pillar_block_hash};
}

h256 PbftManager::getProposal(const std::shared_ptr<PbftVote> &vote) const {
  auto lowest_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), 1);
  for (uint64_t i = 2; i <= vote->getWeight(); ++i) {
    auto tmp_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), i);
    if (lowest_hash > tmp_hash) {
      lowest_hash = tmp_hash;
    }
  }
  return lowest_hash;
}

std::optional<std::pair<std::shared_ptr<PbftBlock>, std::shared_ptr<PbftVote>>> PbftManager::identifyLeaderBlock(
    ProposedBlocks &propose_blocks, std::vector<std::shared_ptr<PbftVote>> &&propose_votes) {
  if (propose_votes.empty()) {
    return {};
  }

  // Each leader candidate with <vote_signature_hash, vote>
  std::map<h256, std::shared_ptr<PbftVote>> leader_candidates;

  for (auto &&v : propose_votes) {
    const auto proposed_block_hash = v->getBlockHash();
    if (proposed_block_hash == kNullBlockHash) {
      LOG(log_er_) << "Propose block hash should not be NULL. Vote " << v;
      continue;
    }

    leader_candidates[getProposal(v)] = std::move(v);
  }

  std::optional<std::pair<std::shared_ptr<PbftBlock>, std::shared_ptr<PbftVote>>> empty_leader_block_data;
  for (auto const &leader_vote : leader_candidates) {
    const auto proposed_block_hash = leader_vote.second->getBlockHash();

    if (pbft_chain_->findPbftBlockInChain(proposed_block_hash)) {
      continue;
    }

    auto leader_block = getValidPbftProposedBlock(propose_blocks, leader_vote.second->getPeriod(), proposed_block_hash);
    if (!leader_block) {
      LOG(log_er_) << "Unable to get proposed block " << proposed_block_hash;
      continue;
    }

    if (leader_block->getPivotDagBlockHash() == kNullBlockHash) {
      if (!empty_leader_block_data.has_value()) {
        empty_leader_block_data = std::make_pair(leader_block, leader_vote.second);
      }

      continue;
    }

    return std::make_pair(leader_block, leader_vote.second);
  }

  // no eligible leader
  return empty_leader_block_data;
}

PbftStateRootValidation PbftManager::validateFinalChainHash(const std::shared_ptr<PbftBlock> &pbft_block) const {
  const auto period = pbft_block->getPeriod();
  const auto &pbft_block_hash = pbft_block->getBlockHash();

  auto expected_final_chain_hash = final_chain_->finalChainHash(period);
  if (!expected_final_chain_hash) {
    LOG(log_wr_) << "Block " << pbft_block_hash << " could not be validated as we are behind";
    return PbftStateRootValidation::Missing;
  }
  if (pbft_block->getFinalChainHash() != expected_final_chain_hash) {
    LOG(log_er_) << "Block " << period << " hash " << pbft_block_hash << " state root "
                 << pbft_block->getFinalChainHash() << " isn't matching actual " << expected_final_chain_hash.value();
    return PbftStateRootValidation::Invalid;
  }

  return PbftStateRootValidation::Valid;
}

bool PbftManager::validatePbftBlockExtraData(const std::shared_ptr<PbftBlock> &pbft_block) const {
  const auto extra_data = pbft_block->getExtraData();
  const auto block_period = pbft_block->getPeriod();
  if (kGenesisConfig.state.hardforks.ficus_hf.isFicusHardfork(block_period)) {
    if (!extra_data.has_value()) {
      LOG(log_er_) << "PBFT block " << pbft_block->getBlockHash() << ", period " << block_period
                   << " does not contain extra data";
      return false;
    }

    // Validate optional pillar block hash
    const auto pillar_block_hash = extra_data->getPillarBlockHash();
    if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(block_period)) {
      if (!pillar_block_hash.has_value()) {
        LOG(log_er_) << "PBFT block " << pbft_block->getBlockHash() << ", period " << block_period
                     << " does not contain pillar block hash";
        return false;
      }
    } else if (pillar_block_hash.has_value()) {
      LOG(log_er_) << "PBFT block " << pbft_block->getBlockHash() << ", period " << block_period
                   << " contains pillar block hash even though it should not";
      return false;
    }

  } else if (extra_data.has_value()) {
    LOG(log_er_) << "PBFT block " << pbft_block->getBlockHash() << ", period " << block_period
                 << " contains extra data even though it should not";
    return false;
  }

  return true;
}

bool PbftManager::validatePillarDataInPeriodData(const PeriodData &period_data) const {
  if (!validatePbftBlockExtraData(period_data.pbft_blk)) {
    return false;
  }

  const auto block_period = period_data.pbft_blk->getPeriod();

  // Validate optional pillar votes presence
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(block_period)) {
    if (!period_data.pillar_votes_.has_value()) {
      LOG(log_er_) << "Sync PBFT block " << period_data.pbft_blk->getBlockHash() << ", period " << block_period
                   << " does not contain pillar votes";
      return false;
    }
  } else if (period_data.pillar_votes_.has_value()) {
    LOG(log_er_) << "Sync PBFT block " << period_data.pbft_blk->getBlockHash() << ", period "
                 << period_data.pbft_blk->getPeriod() << " contains pillar votes even though it should not";
    return false;
  }

  return true;
}

bool PbftManager::validatePbftBlock(const std::shared_ptr<PbftBlock> &pbft_block) const {
  if (!pbft_block) {
    LOG(log_er_) << "Unable to validate pbft block - no block provided";
    return false;
  }

  // Validates pbft_block's previous block hash against pbft chain
  if (!pbft_chain_->checkPbftBlockValidation(pbft_block)) {
    return false;
  }

  auto const &pbft_block_hash = pbft_block->getBlockHash();

  if (validateFinalChainHash(pbft_block) != PbftStateRootValidation::Valid) {
    return false;
  }

  // Validates reward votes
  if (!vote_mgr_->checkRewardVotes(pbft_block, false).first) {
    LOG(log_er_) << "Failed verifying reward votes for proposed PBFT block " << pbft_block_hash;
    return false;
  }

  if (!validatePbftBlockExtraData(pbft_block)) {
    return false;
  }

  // Validate optional pillar block hash
  const auto block_period = pbft_block->getPeriod();
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(block_period)) {
    const auto current_pillar_block = pillar_chain_mgr_->getCurrentPillarBlock();
    if (!current_pillar_block) {
      // This should never happen
      LOG(log_er_) << "Unable to validate PBFT block " << pbft_block_hash << ", period " << block_period
                   << ". No current pillar block present in node";
      return false;
    }

    if (*pbft_block->getExtraData()->getPillarBlockHash() != current_pillar_block->getHash()) {
      LOG(log_er_) << "PBFT block " << pbft_block_hash << " with period " << pbft_block->getPeriod()
                   << " contains pillar block hash " << *pbft_block->getExtraData()->getPillarBlockHash()
                   << ", which is different than the local current pillar block" << current_pillar_block->getHash()
                   << " with period " << current_pillar_block->getPeriod();
      return false;
    }
  }

  auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
  if (anchor_hash == kNullBlockHash) {
    return true;
  }

  auto dag_order_it = anchor_dag_block_order_cache_.find(anchor_hash);
  if (dag_order_it != anchor_dag_block_order_cache_.end()) {
    return true;
  }

  auto dag_blocks_order = dag_mgr_->getDagBlockOrder(anchor_hash, pbft_block->getPeriod());
  if (dag_blocks_order.empty()) {
    LOG(log_er_) << "Missing dag blocks for proposed PBFT block " << pbft_block_hash;
    return false;
  }

  auto calculated_order_hash = calculateOrderHash(dag_blocks_order);
  if (calculated_order_hash != pbft_block->getOrderHash()) {
    LOG(log_er_) << "Order hash incorrect. Pbft block: " << pbft_block_hash
                 << ". Order hash: " << pbft_block->getOrderHash() << " . Calculated hash:" << calculated_order_hash
                 << ". Dag order: " << dag_blocks_order;
    return false;
  }

  anchor_dag_block_order_cache_[anchor_hash].reserve(dag_blocks_order.size());
  for (auto const &dag_blk_hash : dag_blocks_order) {
    auto dag_block = dag_mgr_->getDagBlock(dag_blk_hash);
    assert(dag_block);
    anchor_dag_block_order_cache_[anchor_hash].emplace_back(std::move(dag_block));
  }

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    auto ghost = dag_mgr_->getGhostPath(prev_pbft_block.getPivotDagBlockHash());
    if (ghost.size() > 1 && anchor_hash != ghost[1]) {
      if (!checkBlockWeight(anchor_dag_block_order_cache_[anchor_hash], block_period)) {
        LOG(log_er_) << "PBFT block " << pbft_block_hash << " weight exceeded max limit";
        anchor_dag_block_order_cache_.erase(anchor_hash);
        return false;
      }
    }
  }

  return true;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(const std::shared_ptr<PbftBlock> &pbft_block,
                                                   std::vector<std::shared_ptr<PbftVote>> &&current_round_cert_votes) {
  PeriodData period_data;
  period_data.pbft_blk = pbft_block;
  if (pbft_block->getPivotDagBlockHash() != kNullBlockHash) {
    auto dag_order_it = anchor_dag_block_order_cache_.find(pbft_block->getPivotDagBlockHash());
    assert(dag_order_it != anchor_dag_block_order_cache_.end());
    std::unordered_set<trx_hash_t> trx_set;
    std::vector<trx_hash_t> transactions_to_query;
    period_data.dag_blocks.reserve(dag_order_it->second.size());
    for (const auto &dag_blk : dag_order_it->second) {
      for (const auto &trx_hash : dag_blk->getTrxs()) {
        if (trx_set.insert(trx_hash).second) {
          transactions_to_query.emplace_back(trx_hash);
        }
      }
      period_data.dag_blocks.emplace_back(dag_blk);
    }
    period_data.transactions = trx_mgr_->getNonfinalizedTrx(transactions_to_query);
  }

  auto reward_votes = vote_mgr_->checkRewardVotes(period_data.pbft_blk, true);
  if (!reward_votes.first) {
    LOG(log_er_) << "Missing reward votes in cert voted block " << pbft_block->getBlockHash();
    return false;
  }
  period_data.previous_block_cert_votes = std::move(reward_votes.second);

  if (!pushPbftBlock_(std::move(period_data), std::move(current_round_cert_votes))) {
    LOG(log_er_) << "Failed push cert voted block " << pbft_block->getBlockHash() << " into PBFT chain";
    return false;
  }

  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain() {
  auto net = network_.lock();
  if (!net) {
    LOG(log_er_) << "Failed to obtain net !";
    return;
  }

  sync_queue_.cleanOldData(getPbftPeriod());
  while (periodDataQueueSize() > 0) {
    auto period_data_opt = processPeriodData();
    if (!period_data_opt) continue;

    auto period_data = std::move(*period_data_opt);
    const auto pbft_block_period = period_data.first.pbft_blk->getPeriod();
    const auto pbft_block_hash = period_data.first.pbft_blk->getBlockHash();
    LOG(log_nf_) << "Picked sync block " << pbft_block_hash << " with period " << pbft_block_period;

    if (pushPbftBlock_(std::move(period_data.first), std::move(period_data.second))) {
      LOG(log_dg_) << "Pushed synced PBFT block " << pbft_block_hash << " with period " << pbft_block_period;
      net->setSyncStatePeriod(pbftSyncingPeriod());
    } else {
      LOG(log_er_) << "Failed push PBFT block " << pbft_block_hash << " with period " << pbft_block_period;
      break;
    }
  }
}

void PbftManager::reorderTransactions(SharedTransactions &transactions) {
  // DAG reordering can cause transactions from same sender to be reordered by nonce. If this is the case only
  // transactions from these accounts are sorted and reordered, all other transactions keep the order
  SharedTransactions ordered_transactions;

  // Account with reverse order nonce, the value in a map is a position of last instance
  // of transaction with this account
  std::unordered_map<addr_t, uint32_t> account_reverse_order;

  // While iterating over transactions, account_nonce will keep the last nonce for the account
  std::unordered_map<addr_t, val_t> account_nonce;

  // Find accounts that need reordering and place in account_reverse_order set
  for (uint32_t i = 0; i < transactions.size(); i++) {
    const auto &t = transactions[i];
    auto ro_it = account_reverse_order.find(t->getSender());
    if (ro_it == account_reverse_order.end()) {
      auto it = account_nonce.find(t->getSender());
      if (it == account_nonce.end() || it->second < t->getNonce()) {
        account_nonce[t->getSender()] = t->getNonce();
      } else if (it->second > t->getNonce()) {
        // Nonce of the transaction is smaller than previous nonce, this account transactions will need reordering
        account_reverse_order.insert({t->getSender(), i});
      }
    } else {
      ro_it->second = i;
    }
  }

  // If account_reverse_order size is 0, there is no need to reorder transactions
  if (account_reverse_order.size() > 0) {
    std::unordered_map<addr_t, std::multimap<val_t, std::shared_ptr<Transaction>>> account_nonce_transactions;
    // Keep the order for all transactions that do not need reordering
    for (uint32_t i = 0; i < transactions.size(); i++) {
      const auto &t = transactions[i];
      auto ro_it = account_reverse_order.find(t->getSender());
      if (ro_it != account_reverse_order.end()) {
        account_nonce_transactions[t->getSender()].insert({t->getNonce(), t});
        if (ro_it->second == i) {
          // This is the last instance of transaction for this account, place all the reordered transactions for this
          // account at this position
          for (const auto &nonce : account_nonce_transactions[t->getSender()]) {
            ordered_transactions.push_back(nonce.second);
          }
        }
      } else {
        ordered_transactions.push_back(t);
      }
    }
    transactions = ordered_transactions;
  }
}

void PbftManager::finalize_(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes,
                            uint32_t blocks_per_year, bool synchronous_processing) {
  std::shared_ptr<DagBlock> anchor_block = nullptr;

  if (const auto anchor = period_data.pbft_blk->getPivotDagBlockHash()) {
    anchor_block = dag_mgr_->getDagBlock(anchor);
    if (!anchor_block) {
      LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor << " in DB.";
      assert(false);
    }
  }

  auto result = final_chain_->finalize(std::move(period_data), std::move(finalized_dag_blk_hashes), blocks_per_year,
                                       std::move(anchor_block));

  if (synchronous_processing) {
    result.wait();
  }
}

bool PbftManager::pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<PbftVote>> &&cert_votes) {
  auto const &pbft_block_hash = period_data.pbft_blk->getBlockHash();
  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_nf_) << "PBFT block: " << pbft_block_hash << " in DB already.";
    if (cert_voted_block_for_round_.has_value() && (*cert_voted_block_for_round_)->getBlockHash() == pbft_block_hash) {
      LOG(log_er_) << "Last cert voted value should be kNullBlockHash. Block hash "
                   << (*cert_voted_block_for_round_)->getBlockHash() << " has been pushed into chain already";
      assert(false);
    }
    return false;
  }

  auto pbft_period = period_data.pbft_blk->getPeriod();

  // To finalize the pbft block that includes pillar block hash, pillar block needs to be finalized first
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(pbft_period)) {
    // Note: presence of pillar block hash in extra data was already validated in validatePbftBlock
    const auto pillar_block_hash = period_data.pbft_blk->getExtraData()->getPillarBlockHash();

    // Finalize included pillar block
    auto above_threshold_pillar_votes = pillar_chain_mgr_->finalizePillarBlock(*pillar_block_hash);
    if (above_threshold_pillar_votes.empty()) {
      LOG(log_er_) << "Cannot push PBFT block " << period_data.pbft_blk->getBlockHash() << ", period " << pbft_period
                   << ": Unable to finalize included pillar block " << *pillar_block_hash;
      return false;
    }

    // Save pillar votes into period data
    period_data.pillar_votes_ = std::move(above_threshold_pillar_votes);
  }

  assert(cert_votes.empty() == false);
  const auto sample_cert_vote = cert_votes[0];
  assert(pbft_block_hash == sample_cert_vote->getBlockHash());

  auto null_anchor = period_data.pbft_blk->getPivotDagBlockHash() == kNullBlockHash;

  LOG(log_dg_) << "Storing pbft blk " << pbft_block_hash << " cert votes: " << cert_votes;

  // Update PBFT chain head block
  auto batch = db_->createWriteBatch();
  db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStrForBlock(pbft_block_hash, null_anchor),
                          batch);

  vec_blk_t dag_blocks_order;
  dag_blocks_order.reserve(period_data.dag_blocks.size());
  std::transform(period_data.dag_blocks.begin(), period_data.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                 [](const auto &dag_block) { return dag_block->getHash(); });

  // We need to reorder transactions before saving them
  reorderTransactions(period_data.transactions);

  db_->savePeriodData(period_data, batch);

  // Replace current reward votes
  vote_mgr_->resetRewardVotes(sample_cert_vote->getPeriod(), sample_cert_vote->getRound(), sample_cert_vote->getStep(),
                              sample_cert_vote->getBlockHash(), batch);

  // pass pbft with dag blocks and transactions to adjust difficulty
  if (period_data.pbft_blk->getPivotDagBlockHash() != kNullBlockHash) {
    dag_mgr_->sortitionParamsManager().pbftBlockPushed(period_data, batch,
                                                       pbft_chain_->getPbftChainSizeExcludingEmptyPbftBlocks() + 1);
  }
  {
    // This makes sure that no DAG block or transaction can be added or change state in transaction and dag manager
    // when finalizing pbft block with dag blocks and transactions
    std::unique_lock dag_lock(dag_mgr_->getDagMutex());
    std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());

    // Commit DB
    db_->commitWriteBatch(batch);

    // Set DAG blocks period
    auto const &anchor_hash = period_data.pbft_blk->getPivotDagBlockHash();
    dag_mgr_->setDagBlockOrder(anchor_hash, pbft_period, dag_blocks_order);

    trx_mgr_->updateFinalizedTransactionsStatus(period_data);

    // update PBFT chain size
    pbft_chain_->updatePbftChain(pbft_block_hash, anchor_hash);
  }

  // anchor_dag_block_order_cache_ is valid in one period, clear when period changes
  anchor_dag_block_order_cache_.clear();

  LOG(log_nf_) << "Pushed new PBFT block " << pbft_block_hash << " into chain. Period: " << pbft_period
               << ", round: " << getPbftRound();

  uint32_t blocks_per_year{0};
  // Dynamic lambda was introduced in cacti hardfork -> it affects the number of blocks generated per year, which
  // affects rewards distribution
  if (kGenesisConfig.state.hardforks.isOnCactiHardfork(pbft_period)) {
    blocks_per_year = kGenesisConfig.calcBlocksPerYear(
        static_cast<uint32_t>(current_round_lambda_.count()),
        kGenesisConfig.state.hardforks.cacti_hf
            .consensus_delay /* approx time it takes to receive 2t+1 soft and cert votes after 2*lambda */);

    // Adjust dynamic lambda
    adjustDynamicLambda(pbft_period, sample_cert_vote->getRound());
  } else {
    blocks_per_year = kGenesisConfig.state.dpos.blocks_per_year;
  }

  finalize_(std::move(period_data), std::move(dag_blocks_order), blocks_per_year);

  db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  executed_pbft_block_ = true;

  // Advance pbft consensus period
  advancePeriod();

  // Create new pillar block
  // !!! Important: processPillarBlock must be called only after advancePeriod()
  if (kGenesisConfig.state.hardforks.ficus_hf.isPillarBlockPeriod(pbft_period)) {
    assert(pbft_period == pbft_chain_->getPbftChainSize());
    processPillarBlock(pbft_period);
  }

  return true;
}

void PbftManager::processPillarBlock(PbftPeriod current_pbft_chain_size) {
  // Pillar block use state from current_pbft_chain_size - final_chain_->delegationDelay(), e.g. block with period 32
  // uses state from period 27.
  PbftPeriod request_period = current_pbft_chain_size - final_chain_->delegationDelay();
  // advancePeriod() -> resetConsensus() -> waitForPeriodFinalization() makes sure block request_period was already
  // finalized
  assert(final_chain_->lastBlockNumber() >= request_period);

  const auto block_header = final_chain_->blockHeader(request_period);
  const auto bridge_root = final_chain_->getBridgeRoot(request_period);
  const auto bridge_epoch = final_chain_->getBridgeEpoch(request_period);

  // Create pillar block
  const auto pillar_block =
      pillar_chain_mgr_->createPillarBlock(current_pbft_chain_size, block_header, bridge_root, bridge_epoch);

  // Optimization - creates pillar vote right after pillar block was created, otherwise pillar votes are created during
  // next period pbft voting
  if (pillar_block) {
    for (const auto &wallet : eligible_wallets_.getWallets(current_pbft_chain_size + 1)) {
      // Wallet is not dpos eligible - do no vote
      if (!wallet.first) {
        continue;
      }

      // Pillar votes are created in the next period, this is optimization to create & broadcast it a bit faster
      const auto pillar_vote = pillar_chain_mgr_->genAndPlacePillarVote(
          current_pbft_chain_size + 1, pillar_block->getHash(), wallet.second.node_secret, periodDataQueueEmpty());
      if (pillar_vote) {
        last_placed_pillar_vote_period_ = pillar_vote->getPeriod();
      }
    }
  }
}

PbftPeriod PbftManager::pbftSyncingPeriod() const {
  return std::max(sync_queue_.getPeriod(), pbft_chain_->getPbftChainSize());
}

std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<PbftVote>>>> PbftManager::processPeriodData() {
  auto [period_data, cert_votes, node_id] = sync_queue_.pop();
  auto pbft_block_hash = period_data.pbft_blk->getBlockHash();
  LOG(log_dg_) << "Pop pbft block " << pbft_block_hash << " with period " << period_data.pbft_blk->getPeriod()
               << " from synced queue";

  if (pbft_chain_->findPbftBlockInChain(pbft_block_hash)) {
    LOG(log_dg_) << "PBFT block " << pbft_block_hash << " already present in chain.";
    return std::nullopt;
  }

  auto net = network_.lock();
  assert(net);  // Should never happen

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();

  // Check previous hash matches
  if (period_data.pbft_blk->getPrevBlockHash() != last_pbft_block_hash) {
    auto last_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    if (period_data.pbft_blk->getPeriod() <= last_pbft_block.getPeriod()) {
      // Old block in the sync queue
      return std::nullopt;
    }

    LOG(log_er_) << "Invalid PBFT block " << pbft_block_hash
                 << "; prevHash: " << period_data.pbft_blk->getPrevBlockHash() << " from peer " << node_id.abridged()
                 << " received, stop syncing.";
    sync_queue_.clear();

    // Handle malicious peer on network level
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  bool retry_logged = false;
  while (true) {
    auto validation_result = validateFinalChainHash(period_data.pbft_blk);
    if (validation_result != PbftStateRootValidation::Missing) {
      if (validation_result == PbftStateRootValidation::Invalid) {
        LOG(log_er_) << "Failed verifying block " << pbft_block_hash
                     << " with invalid state root: " << period_data.pbft_blk->getFinalChainHash()
                     << ". Disconnect malicious peer " << node_id.abridged();
        sync_queue_.clear();
        net->handleMaliciousSyncPeer(node_id);
        return std::nullopt;
      }
      break;
    }
    // If syncing and pbft manager is faster than execution a delay might be needed to allow EVM to catch up
    final_chain_->waitForFinalized();
    if (!retry_logged) {
      LOG(log_wr_) << "PBFT block " << pbft_block_hash
                   << " validation delayed, state root missing, execution is behind";
      retry_logged = true;
    }
  }

  // Check reward votes
  auto reward_votes = vote_mgr_->checkRewardVotes(period_data.pbft_blk, true);
  if (!reward_votes.first) {
    LOG(log_er_) << "Failed verifying reward votes for block " << pbft_block_hash << ". Disconnect malicious peer "
                 << node_id.abridged();
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  // Special case when previous block was already in chain so we hit condition
  // pbft_chain_->findPbftBlockInChain(pbft_block_hash) and it's cert votes were not verified here, they are part of
  // vote_manager so we need to replace them as they are not verified period_data structure
  if (period_data.previous_block_cert_votes.size() && !period_data.previous_block_cert_votes.front()->getWeight()) {
    period_data.previous_block_cert_votes = std::move(reward_votes.second);
  }

  // Validate cert votes
  if (!validatePbftBlockCertVotes(period_data.pbft_blk, cert_votes)) {
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash
                 << " doesn't have enough valid cert votes. Clear synced PBFT blocks!";
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  std::unordered_set<trx_hash_t> trx_set_period_data;
  for (auto const &transaction : period_data.transactions) {
    trx_set_period_data.emplace(transaction->getHash());
  }

  std::unordered_set<trx_hash_t> trx_set;
  std::vector<trx_hash_t> finalized_transactions_to_check;
  for (auto const &dag_block : period_data.dag_blocks) {
    for (auto const &trx_hash : dag_block->getTrxs()) {
      if (trx_set.insert(trx_hash).second && !trx_set_period_data.contains(trx_hash)) {
        finalized_transactions_to_check.emplace_back(trx_hash);
      }
    }
  }

  // Verify period data is not missing any transaction
  auto non_finalized_transactions = trx_mgr_->excludeFinalizedTransactions(finalized_transactions_to_check);
  if (non_finalized_transactions.size() > 0) {
    for (auto const &t : non_finalized_transactions) {
      LOG(log_er_) << "Synced PBFT block " << pbft_block_hash << " has missing transaction " << t;
    }
  }

  // Verify period data does not contain any finalized transactions
  if (!trx_mgr_->verifyTransactionsNotFinalized(period_data.transactions)) {
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash << " has finalized transactions";
  }

  if (!validatePillarDataInPeriodData(period_data)) {
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  const auto block_period = period_data.pbft_blk->getPeriod();
  // Validate pillar votes
  if (kGenesisConfig.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(block_period) &&
      !validatePbftBlockPillarVotes(period_data)) {
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash << ", period " << block_period
                 << " doesn't have enough valid pillar votes. Clear synced PBFT blocks!";
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  return std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<PbftVote>>>>(
      {std::move(period_data), std::move(cert_votes)});
}

bool PbftManager::validatePbftBlockCertVotes(const std::shared_ptr<PbftBlock> pbft_block,
                                             const std::vector<std::shared_ptr<PbftVote>> &cert_votes) const {
  // To speed up syncing/rebuilding full strict vote verification is done for all votes on every
  // full_vote_validation_interval and for a random vote for each block
  const uint32_t full_vote_validation_interval = 100;
  const uint32_t vote_to_validate = std::rand() % cert_votes.size();
  const bool strict_validation = (pbft_block->getPeriod() % full_vote_validation_interval == 0);

  if (cert_votes.empty()) {
    LOG(log_er_) << "No cert votes provided! The synced PBFT block comes from a malicious player";
    return false;
  }

  size_t votes_weight = 0;
  auto first_vote_round = cert_votes[0]->getRound();
  auto first_vote_period = cert_votes[0]->getPeriod();

  if (pbft_block->getPeriod() != first_vote_period) {
    LOG(log_er_) << "pbft block period " << pbft_block->getPeriod() << " != first_vote_period " << first_vote_period;
    return false;
  }

  for (uint32_t vote_counter = 0; vote_counter < cert_votes.size(); vote_counter++) {
    const auto &v = cert_votes[vote_counter];
    // Any info is wrong that can determine the synced PBFT block comes from a malicious player
    if (v->getPeriod() != first_vote_period) {
      LOG(log_er_) << "Invalid cert vote " << v->getHash() << " period " << v->getPeriod() << ", PBFT block "
                   << pbft_block->getBlockHash() << ", first_vote_period " << first_vote_period;
      return false;
    }

    if (v->getRound() != first_vote_round) {
      LOG(log_er_) << "Invalid cert vote " << v->getHash() << " round " << v->getRound() << ", PBFT block "
                   << pbft_block->getBlockHash() << ", first_vote_round " << first_vote_round;
      return false;
    }

    if (v->getType() != PbftVoteTypes::cert_vote) {
      LOG(log_er_) << "Invalid cert vote " << v->getHash() << " type " << static_cast<uint8_t>(v->getType())
                   << ", PBFT block " << pbft_block->getBlockHash();
      return false;
    }

    if (v->getStep() != PbftStates::certify_state) {
      LOG(log_er_) << "Invalid cert vote " << v->getHash() << " step " << v->getStep() << ", PBFT block "
                   << pbft_block->getBlockHash();
      return false;
    }

    if (v->getBlockHash() != pbft_block->getBlockHash()) {
      LOG(log_er_) << "Invalid cert vote " << v->getHash() << " block hash " << v->getBlockHash() << ", PBFT block "
                   << pbft_block->getBlockHash();
      return false;
    }

    bool strict = strict_validation || (vote_counter == vote_to_validate);

    if (const auto ret = vote_mgr_->validateVote(v, strict); !ret.first) {
      LOG(log_er_) << "Cert vote " << v->getHash() << " validation failed. Err: " << ret.second << ", pbft block "
                   << pbft_block->getBlockHash();
      return false;
    }

    assert(v->getWeight());
    votes_weight += *v->getWeight();

    vote_mgr_->addVerifiedVote(v);
  }

  const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(first_vote_period - 1, PbftVoteTypes::cert_vote);
  if (!two_t_plus_one.has_value()) {
    return false;
  }

  if (votes_weight < *two_t_plus_one) {
    LOG(log_wr_) << "Invalid votes weight " << votes_weight << " < two_t_plus_one " << *two_t_plus_one
                 << ", pbft block " << pbft_block->getBlockHash();
    return false;
  }

  return true;
}

bool PbftManager::validatePbftBlockPillarVotes(const PeriodData &period_data) const {
  if (!period_data.pillar_votes_.has_value() || period_data.pillar_votes_->empty()) {
    LOG(log_er_) << "No pillar votes provided, pbft block period " << period_data.pbft_blk->getPeriod()
                 << ". The synced PBFT block comes from a malicious player";
    return false;
  }

  const auto &pbft_block_hash = period_data.pbft_blk->getBlockHash();
  const auto required_votes_period = period_data.pbft_blk->getPeriod();

  const auto current_pillar_block = pillar_chain_mgr_->getCurrentPillarBlock();
  if (current_pillar_block->getPeriod() + 1 != required_votes_period) {
    LOG(log_er_) << "Sync pillar votes required period " << required_votes_period
                 << " != " << " current pillar block period " << current_pillar_block->getPeriod() << " + 1";
    return false;
  }

  uint64_t votes_weight = 0;
  for (auto &vote : *period_data.pillar_votes_) {
    // Any info is wrong that can determine the synced PBFT block comes from a malicious player
    if (vote->getPeriod() != required_votes_period) {
      LOG(log_er_) << "Invalid sync pillar vote " << vote->getHash() << " period " << vote->getPeriod()
                   << ", PBFT block " << pbft_block_hash << ", kRequiredVotesPeriod " << required_votes_period;
      return false;
    }

    if (vote->getBlockHash() != current_pillar_block->getHash()) {
      LOG(log_er_) << "Invalid sync pillar vote " << vote->getHash() << ", vote period " << vote->getPeriod()
                   << ", vote pillar block hash " << vote->getBlockHash()
                   << ", current pillar block hash: " << current_pillar_block->getHash()
                   << ", current pillar block period " << current_pillar_block->getPeriod()
                   << ", full data: " << current_pillar_block->getJson();
      return false;
    }

    if (!pillar_chain_mgr_->validatePillarVote(vote)) {
      LOG(log_er_) << "Invalid sync pillar vote " << vote->getHash();
      return false;
    }

    if (const auto vote_weight = pillar_chain_mgr_->addVerifiedPillarVote(vote); vote_weight) {
      votes_weight += vote_weight;
    } else {
      LOG(log_er_) << "Unable to add sync pillar vote " << vote->getHash();
      return false;
    }
  }

  const auto pillar_consensus_threshold = pillar_chain_mgr_->getPillarConsensusThreshold(required_votes_period - 1);
  if (!pillar_consensus_threshold.has_value()) {
    LOG(log_er_) << "Unable to obtain pillar consensus threshold for period " << required_votes_period - 1;
    return false;
  }

  if (votes_weight < *pillar_consensus_threshold) {
    LOG(log_wr_) << "Invalid sync pillar votes weight " << votes_weight << " < threshold "
                 << *pillar_consensus_threshold << ", period " << required_votes_period - 1;
    return false;
  }

  return true;
}

bool PbftManager::canParticipateInConsensus(PbftPeriod period, const addr_t &node_addr) const {
  try {
    return final_chain_->dposIsEligible(period, node_addr);
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_er_) << "Unable to decide if node is consensus node or not for period: " << period
                 << ". Period is too far ahead of actual finalized pbft chain size (" << final_chain_->lastBlockNumber()
                 << "). Err msg: " << e.what()
                 << ". Node is considered as not eligible to participate in consensus for period " << period;
  }

  return false;
}

std::map<PbftPeriod, std::vector<std::shared_ptr<PbftBlock>>> PbftManager::getProposedBlocks() const {
  return proposed_blocks_.getProposedBlocks();
}

blk_hash_t PbftManager::lastPbftBlockHashFromQueueOrChain() {
  auto pbft_block = sync_queue_.lastPbftBlock();
  if (pbft_block && pbft_block->getPeriod() >= getPbftPeriod()) {
    return pbft_block->getBlockHash();
  }
  return pbft_chain_->getLastPbftBlockHash();
}

bool PbftManager::periodDataQueueEmpty() const { return sync_queue_.empty(); }

void PbftManager::periodDataQueuePush(PeriodData &&period_data, dev::p2p::NodeID const &node_id,
                                      std::vector<std::shared_ptr<PbftVote>> &&current_block_cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();

  // Only do parallel transactions retrieve for blocks bigger than 100 transactions
  auto trx_size = period_data.transactions.size();
  if (trx_size > 100) {
    auto chunk_size = trx_size / kSyncingThreadPoolSize;

    std::vector<std::future<void>> futures;
    futures.reserve(kSyncingThreadPoolSize);
    // Launch tasks in parallel
    for (uint32_t i = 0; i < kSyncingThreadPoolSize; ++i) {
      futures.push_back(sync_thread_pool_->post([&period_data, i, chunk_size, trx_size]() {
        const uint32_t start = i * chunk_size;
        const uint32_t end = std::min((i + 1) * chunk_size, trx_size);
        for (uint32_t j = start; j < end; j++) period_data.transactions[j]->getSender();
      }));
    }
    for (uint32_t i = 0; i < kSyncingThreadPoolSize; ++i) {
      futures[i].get();
    }
  }

  if (!sync_queue_.push(std::move(period_data), node_id, pbft_chain_->getPbftChainSize(),
                        std::move(current_block_cert_votes))) {
    LOG(log_er_) << "Trying to push period data with " << period << " period, but current period is "
                 << sync_queue_.getPeriod();
  }
}

size_t PbftManager::periodDataQueueSize() const { return sync_queue_.size(); }

bool PbftManager::checkBlockWeight(const std::vector<std::shared_ptr<DagBlock>> &dag_blocks, PbftPeriod period) const {
  const u256 total_weight =
      std::accumulate(dag_blocks.begin(), dag_blocks.end(), u256(0),
                      [](u256 value, const auto &dag_block) { return value + dag_block->getGasEstimation(); });
  const auto pbft_gas_limit = kGenesisConfig.getGasLimits(period).second;
  if (total_weight > pbft_gas_limit) {
    return false;
  }
  return true;
}

blk_hash_t PbftManager::getLastPbftBlockHash() { return pbft_chain_->getLastPbftBlockHash(); }

std::shared_ptr<PbftBlock> PbftManager::getPbftProposedBlock(PbftPeriod period, const blk_hash_t &block_hash) const {
  auto proposed_block = proposed_blocks_.getPbftProposedBlock(period, block_hash);
  if (!proposed_block.has_value()) {
    return nullptr;
  }

  return proposed_block->first;
}

PbftManager::EligibleWallets::EligibleWallets(const std::vector<WalletConfig> &wallets) {
  wallets_.reserve(wallets.size());
  for (const auto &wallet : wallets) {
    wallets_.emplace_back(false, wallet);
  }
}

void PbftManager::EligibleWallets::updateWalletsEligibility(
    PbftPeriod period, const std::shared_ptr<final_chain::FinalChain> &final_chain) {
  assert(period > period_ || period == 0);
  assert(period <= final_chain->lastBlockNumber() + final_chain->delegationDelay());

  for (auto &wallet : wallets_) {
    wallet.first = final_chain->dposIsEligible(period, wallet.second.node_addr);
  }

  period_ = period;
}

const std::vector<std::pair<bool, WalletConfig>> &PbftManager::EligibleWallets::getWallets(
    PbftPeriod current_pbft_period) const {
  assert(period_ == current_pbft_period - 1);

  return wallets_;
}

PbftPeriod PbftManager::EligibleWallets::getWalletsEligiblePeriod() const { return period_; }

std::chrono::milliseconds PbftManager::getPbftDeadline() const {
  if (kGenesisConfig.state.hardforks.isOnCactiHardfork(getPbftPeriod())) {
    auto block_propagation = std::chrono::milliseconds(kGenesisConfig.state.hardforks.cacti_hf.block_propagation_min);
    if (getPbftRound() > 1) {
      block_propagation = std::chrono::milliseconds(kGenesisConfig.state.hardforks.cacti_hf.block_propagation_max);
    }

    return std::max(4 * current_round_lambda_, block_propagation);
  }

  return 4 * current_round_lambda_;
}

}  // namespace taraxa
