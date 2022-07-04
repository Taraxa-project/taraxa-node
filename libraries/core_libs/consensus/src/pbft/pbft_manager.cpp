/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "pbft/pbft_manager.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "dag/dag.hpp"
#include "final_chain/final_chain.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/period_data.hpp"
#include "pbft/step/certify.hpp"
#include "pbft/step/filter.hpp"
#include "pbft/step/finish.hpp"
#include "pbft/step/polling.hpp"
#include "pbft/step/propose.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

PbftManager::PbftManager(const PbftConfig &conf, const blk_hash_t &dag_genesis_block_hash, addr_t node_addr,
                         std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
                         std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                         std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
                         secret_t node_sk, vrf_wrapper::vrf_sk_t vrf_sk, uint32_t max_levels_per_period)
    : node_addr_(node_addr),
      dag_genesis_block_hash_(dag_genesis_block_hash),
      config_(conf),
      max_levels_per_period_(max_levels_per_period) {
  LOG_OBJECTS_CREATE("PBFT_MGR");

  shared_state_ = std::make_shared<CommonState>();
  node_ = std::make_shared<NodeFace>(node_addr, node_sk, vrf_sk, std::move(db), next_votes_mgr, pbft_chain, vote_mgr,
                                     dag_mgr, dag_blk_mgr, trx_mgr, final_chain);
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::weak_ptr<Network> network) { node_->network_ = std::move(network); }

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  node_->pbft_manager_ = shared_from_this();

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
  node_->final_chain_->stop();

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
  LOG(log_nf_) << "PBFT running ...";

  for (auto period = node_->final_chain_->last_block_number() + 1, curr_period = node_->pbft_chain_->getPbftChainSize();
       period <= curr_period; ++period) {
    auto period_raw = node_->db_->getPeriodDataRaw(period);
    if (period_raw.size() == 0) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    PeriodData period_data(period_raw);
    if (period_data.pbft_blk->getPeriod() != period) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << period_data.pbft_blk->getBlockHash()
                   << " has different period " << period_data.pbft_blk->getPeriod()
                   << " in block data than in block order db: " << period;
      assert(false);
    }
    // We need this section because votes need to be verified for reward distribution
    for (const auto &v : period_data.previous_block_cert_votes) {
      node_->vote_mgr_->addRewardVote(v);
    }
    if (!node_->vote_mgr_->checkRewardVotes(period_data.pbft_blk)) {
      LOG(log_er_) << "Invalid reward votes in block " << period_data.pbft_blk->getBlockHash() << " in DB.";
      assert(false);
    }
    finalize_(std::move(period_data), node_->db_->getFinalizedDagBlockHashesByPeriod(period), period == curr_period);
  }

  // Initialize PBFT status
  initialize();

  continuousOperation_();
}

// // Only to be used for tests...
// void PbftManager::resume() {
//   // Will only appear in testing...
//   LOG(log_si_) << "Resuming PBFT daemon...";

//   if (step_ == 1) {
//     shared_state_->state_ = value_proposal_state;
//   } else if (step_ == 2) {
//     shared_state_->state_ = filter_state;
//   } else if (step_ == 3) {
//     shared_state_->state_ = certify_state;
//   } else if (step_ % 2 == 0) {
//     shared_state_->state_ = finish_state;
//   } else {
//     shared_state_->state_ = finish_polling_state;
//   }

//   daemon_ = std::make_unique<std::thread>([this]() { continuousOperation_(); });
// }

// // Only to be used for tests...
// void PbftManager::setMaxWaitForSoftVotedBlock_ms(uint64_t wait_ms) {
//   max_wait_for_soft_voted_block_steps_ms_ = wait_ms;
// }

// // Only to be used for tests...
// void PbftManager::setMaxWaitForNextVotedBlock_ms(uint64_t wait_ms) {
//   max_wait_for_next_voted_block_steps_ms_ = wait_ms;
// }

// // Only to be used for tests...
// void PbftManager::resumeSingleState() {
//   if (!stopped_.load()) daemon_->join();
//   stopped_ = false;

//   if (step_ == 1) {
//     shared_state_->state_ = value_proposal_state;
//   } else if (step_ == 2) {
//     shared_state_->state_ = filter_state;
//   } else if (step_ == 3) {
//     shared_state_->state_ = certify_state;
//   } else if (step_ % 2 == 0) {
//     shared_state_->state_ = finish_state;
//   } else {
//     shared_state_->state_ = finish_polling_state;
//   }

//   doNextState_();
// }

// // Only to be used for tests...
// void PbftManager::doNextState_() {
//   auto initial_state = shared_state_->state_;

//   while (!stopped_ && shared_state_->state_ == initial_state) {
//     if (stateOperations_()) {
//       continue;
//     }

//     // PBFT states
//     switch (shared_state_->state_) {
//       case value_proposal_state:
//         proposeBlock_();
//         break;
//       case filter_state:
//         identifyBlock_();
//         break;
//       case certify_state:
//         certifyBlock_();
//         break;
//       case finish_state:
//         firstFinish_();
//         break;
//       case finish_polling_state:
//         secondFinish_();
//         break;
//       default:
//         LOG(log_er_) << "Unknown PBFT state " << shared_state_->state_;
//         assert(false);
//     }

//     nextStep_();
//     if (shared_state_->state_ != initial_state) {
//       return;
//     }
//     sleep_();
//   }
// }

void PbftManager::continuousOperation_() {
  while (!stopped_) {
    if (stateOperations_()) {
      continue;
    }
    current_step_->run();
    if (current_step_->finished()) {
      nextStep_();
    }
    sleep_();
  }
}

std::pair<bool, uint64_t> PbftManager::getDagBlockPeriod(blk_hash_t const &hash) {
  std::pair<bool, uint64_t> res;
  auto value = node_->db_->getDagBlockPeriod(hash);
  if (value == nullptr) {
    res.first = false;
  } else {
    res.first = true;
    res.second = value->first;
  }
  return res;
}

uint64_t PbftManager::getPbftRound() const { return round_; }

void PbftManager::setPbftRound(uint64_t const round) {
  node_->db_->savePbftMgrField(PbftMgrRoundStep::PbftRound, round);
  round_ = round;
}

size_t PbftManager::getSortitionThreshold() const { return sortition_threshold_; }

size_t PbftManager::getTwoTPlusOne() const { return TWO_T_PLUS_ONE; }

// Notice: Test purpose
void PbftManager::setSortitionThreshold(size_t const sortition_threshold) {
  sortition_threshold_ = sortition_threshold;
}

void PbftManager::updateDposState_() {
  dpos_period_ = node_->pbft_chain_->getPbftChainSize();
  do {
    try {
      dpos_votes_count_ = node_->final_chain_->dpos_eligible_total_vote_count(dpos_period_);
      weighted_votes_count_ = node_->final_chain_->dpos_eligible_vote_count(dpos_period_, node_addr_);
      break;
    } catch (state_api::ErrFutureBlock &c) {
      LOG(log_nf_) << c.what();
      LOG(log_nf_) << "PBFT period " << dpos_period_ << " is too far ahead of DPOS, need wait! PBFT chain size "
                   << node_->pbft_chain_->getPbftChainSize() << ", have executed chain size "
                   << node_->final_chain_->last_block_number();
      // Sleep one PBFT lambda time
      thisThreadSleepForMilliSeconds(POLLING_INTERVAL_ms);
    }
  } while (!stopped_);

  LOG(log_nf_) << "DPOS total votes count is " << dpos_votes_count_ << " for period " << dpos_period_ << ". Account "
               << node_addr_ << " has " << weighted_votes_count_ << " weighted votes";
}

size_t PbftManager::getDposTotalVotesCount() const { return dpos_votes_count_.load(); }

size_t PbftManager::getDposWeightedVotesCount() const { return weighted_votes_count_.load(); }

size_t PbftManager::dposEligibleVoteCount_(addr_t const &addr) {
  try {
    return node_->final_chain_->dpos_eligible_vote_count(dpos_period_, addr);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << c.what() << ". Period " << dpos_period_ << " is too far ahead of DPOS";
    return 0;
  }
}

void PbftManager::setPbftStep(size_t const pbft_step) {
  static std::default_random_engine random_engine_{std::random_device{}()};

  node_->db_->savePbftMgrField(PbftMgrRoundStep::PbftStep, pbft_step);
  step_ = pbft_step;

  if (step_ > MAX_STEPS && LAMBDA_backoff_multiple < 8) {
    // Note: We calculate the lambda for a step independently of prior steps
    //       in case missed earlier steps.
    std::uniform_int_distribution<u_long> distribution(0, step_ - MAX_STEPS);
    auto lambda_random_count = distribution(random_engine_);
    LAMBDA_backoff_multiple = 2 * LAMBDA_backoff_multiple;
    LAMBDA_ms = std::min(kMaxLambda, config_.lambda_ms_min * (LAMBDA_backoff_multiple + lambda_random_count));
    LOG(log_dg_) << "Surpassed max steps, exponentially backing off lambda to " << LAMBDA_ms << " ms in round "
                 << getPbftRound() << ", step " << step_;
  } else {
    LAMBDA_ms = config_.lambda_ms_min;
    LAMBDA_backoff_multiple = 1;
  }
}

void PbftManager::resetStep_() {
  step_ = 1;
  startingStepInRound_ = 1;

  LAMBDA_ms = config_.lambda_ms_min;
  LAMBDA_backoff_multiple = 1;
}

bool PbftManager::resetRound_() {
  // Jump to round
  auto consensus_pbft_round = node_->vote_mgr_->roundDeterminedFromVotes(TWO_T_PLUS_ONE);
  // current round
  auto round = getPbftRound();

  if (consensus_pbft_round <= round) {
    return false;
  }

  LOG(log_nf_) << "Determined round from votes: " << consensus_pbft_round;
  shared_state_->round_clock_initial_datetime_ = shared_state_->now_;
  // Update current round and reset step to 1
  round_ = consensus_pbft_round;
  resetStep_();
  shared_state_->state_ = propose;
  current_step_ = std::make_unique<step::Propose>(shared_state_, node_);

  LOG(log_dg_) << "Advancing clock to pbft round " << consensus_pbft_round << ", step 1, and resetting clock.";

  const auto previous_round = consensus_pbft_round - 1;

  auto next_votes = node_->next_votes_manager_->getNextVotes();

  // Update in DB first
  auto batch = node_->db_->createWriteBatch();

  // Update PBFT round and reset step to 1
  node_->db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftRound, consensus_pbft_round, batch);
  node_->db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftStep, 1, batch);

  node_->db_->addPbft2TPlus1ToBatch(previous_round, TWO_T_PLUS_ONE, batch);
  node_->db_->addNextVotesToBatch(previous_round, next_votes, batch);
  if (round > 1) {
    // Cleanup old previoud round next votes
    node_->db_->removeNextVotesToBatch(round - 1, batch);
  }

  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold,
                                            sortition_threshold_, batch);
  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, dpos_period_.load(),
                                            batch);
  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                            getDposTotalVotesCount(), batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::ExecutedInRound, false, batch);
  node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::OwnStartingValueInRound, kNullBlockHash, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound, kNullBlockHash, batch);
  if (shared_state_->soft_voted_block_for_this_round_) {
    // Cleanup soft votes for previous round
    node_->db_->removeSoftVotesToBatch(round, batch);
  }
  node_->db_->commitWriteBatch(batch);

  have_executed_this_round_ = false;
  should_have_cert_voted_in_this_round_ = false;
  // reset starting value to kNullBlockHash
  shared_state_->own_starting_value_for_round_ = kNullBlockHash;
  // reset next voted value since start a new round
  next_voted_null_block_hash_ = false;
  next_voted_soft_value_ = false;
  polling_state_print_log_ = true;

  // Reset soft voting leader value in the new upcoming round
  shared_state_->soft_voted_block_for_this_round_ = kNullBlockHash;

  // Move to a new round, cleanup previous round votes
  node_->vote_mgr_->cleanupVotes(consensus_pbft_round);

  if (executed_pbft_block_) {
    node_->vote_mgr_->removeVerifiedVotes();
    updateDposState_();
    // reset sortition_threshold and TWO_T_PLUS_ONE
    updateTwoTPlusOneAndThreshold_();
    node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
    executed_pbft_block_ = false;
  }
  // Restart while loop...
  return true;
}

void PbftManager::sleep_() {
  shared_state_->now_ = std::chrono::system_clock::now();
  shared_state_->duration_ = shared_state_->now_ - shared_state_->round_clock_initial_datetime_;
  shared_state_->elapsed_time_in_round_ms_ =
      std::chrono::duration_cast<std::chrono::milliseconds>(shared_state_->duration_).count();
  LOG(log_tr_) << "elapsed time in round(ms): " << shared_state_->elapsed_time_in_round_ms_ << ", step " << step_;
  // Add 25ms for practical reality that a thread will not stall for less than 10-25 ms...
  if (shared_state_->next_step_time_ms_ > shared_state_->elapsed_time_in_round_ms_ + 25) {
    auto time_to_sleep_for_ms = shared_state_->next_step_time_ms_ - shared_state_->elapsed_time_in_round_ms_;
    LOG(log_tr_) << "Time to sleep(ms): " << time_to_sleep_for_ms << " in round " << getPbftRound() << ", step "
                 << step_;
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(time_to_sleep_for_ms));
  } else {
    LOG(log_tr_) << "Skipping sleep, running late...";
  }
}

void PbftManager::initialize() {
  // Initial PBFT state

  // Time constants...
  LAMBDA_ms = config_.lambda_ms_min;
  max_wait_for_soft_voted_block_steps_ms_ = MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS * 2 * LAMBDA_ms;
  // max_wait_for_next_voted_block_steps_ms_ = MAX_WAIT_FOR_NEXT_VOTED_BLOCK_STEPS * 2 * LAMBDA_ms;

  auto round = node_->db_->getPbftMgrField(PbftMgrRoundStep::PbftRound);
  auto step = node_->db_->getPbftMgrField(PbftMgrRoundStep::PbftStep);
  if (round == 1 && step == 1) {
    // Node start from scratch
    shared_state_->state_ = propose;
    current_step_ = std::make_unique<step::Propose>(shared_state_, node_);
  } else {
    if (step < 4) {
      // Node start from DB, skip step 1 or 2 or 3
      step = 4;
    }

    if (step % 2 == 0) {
      // Node start from DB in first finishing state
      shared_state_->state_ = finish;
      current_step_ = std::make_unique<step::Finish>(step, shared_state_, node_);
    } else if (step % 2 == 1) {
      // Node start from DB in second finishing state
      shared_state_->state_ = polling;
      current_step_ = std::make_unique<step::Polling>(step, shared_state_, node_);
    }
  }

  // This is used to offset endtime for second finishing step...
  startingStepInRound_ = step;
  setPbftStep(step);
  setPbftRound(round);

  if (round > 1) {
    // Get next votes for previous round from DB
    auto next_votes_in_previous_round = node_->db_->getNextVotes(round - 1);
    if (next_votes_in_previous_round.empty()) {
      LOG(log_er_) << "Cannot get any next votes in previous round " << round - 1 << ". Currrent round " << round
                   << " step " << step;
      assert(false);
    }
    auto previous_round_2t_plus_1 = node_->db_->getPbft2TPlus1(round - 1);
    if (previous_round_2t_plus_1 == 0) {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in previous round " << round - 1 << ". Current round " << round << " step "
                   << step;
      assert(false);
    }
    node_->next_votes_manager_->updateNextVotes(next_votes_in_previous_round, previous_round_2t_plus_1);
  }

  shared_state_->previous_round_next_voted_value_ = node_->next_votes_manager_->getVotedValue();
  previous_round_next_voted_null_block_hash_ = node_->next_votes_manager_->haveEnoughVotesForNullBlockHash();

  LOG(log_nf_) << "Node initialize at round " << round << " step " << step
               << ". Previous round has enough next votes for kNullBlockHash: " << std::boolalpha
               << node_->next_votes_manager_->haveEnoughVotesForNullBlockHash() << ", voted value "
               << shared_state_->previous_round_next_voted_value_ << ", next votes size in previous round is "
               << node_->next_votes_manager_->getNextVotesWeight();

  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  auto own_starting_value = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound);
  if (own_starting_value) {
    // From DB
    shared_state_->own_starting_value_for_round_ = *own_starting_value;
  } else {
    // Default value
    shared_state_->own_starting_value_for_round_ = kNullBlockHash;
  }

  auto soft_voted_block_hash = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound);
  if (soft_voted_block_hash) {
    // From DB
    shared_state_->soft_voted_block_for_this_round_ = *soft_voted_block_hash;
    for (const auto &vote : node_->db_->getSoftVotes(round)) {
      if (vote->getBlockHash() != *soft_voted_block_hash) {
        LOG(log_er_) << "The soft vote should have voting value " << *soft_voted_block_hash << ". Vote" << vote;
        continue;
      }
      node_->vote_mgr_->addVerifiedVote(vote);
    }
  } else {
    // Default value
    shared_state_->soft_voted_block_for_this_round_ = kNullBlockHash;
  }

  // Used for detecting timeout due to malicious node
  // failing to gossip PBFT block or DAG blocks
  // Requires that shared_state_->soft_voted_block_for_this_round_ already be initialized from db
  initializeVotedValueTimeouts_();

  auto last_cert_voted_value = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue);
  if (last_cert_voted_value) {
    shared_state_->last_cert_voted_value_ = *last_cert_voted_value;
    LOG(log_nf_) << "Initialize last cert voted value " << shared_state_->last_cert_voted_value_;
  }

  executed_pbft_block_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::ExecutedBlock);
  have_executed_this_round_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::ExecutedInRound);
  next_voted_soft_value_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue);
  next_voted_null_block_hash_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash);

  shared_state_->round_clock_initial_datetime_ = std::chrono::system_clock::now();
  shared_state_->next_step_time_ms_ = 0;

  updateDposState_();
  // Initialize TWO_T_PLUS_ONE and sortition_threshold
  updateTwoTPlusOneAndThreshold_();
}

void PbftManager::nextStep_() {
  switch (shared_state_->state_) {
    case propose:
      current_step_ = std::make_unique<step::Filter>(shared_state_, node_);
      break;
    case filter:
      current_step_ = std::make_unique<step::Certify>(shared_state_, node_);
      break;
    case certify:
      current_step_ = std::make_unique<step::Finish>(step_ + 1, shared_state_, node_);
      break;
    case finish:
      current_step_ = std::make_unique<step::Polling>(step_ + 1, shared_state_, node_);
      break;
    case polling:
      current_step_ = std::make_unique<step::Finish>(step_ + 1, shared_state_, node_);
      break;
    default:
      LOG(log_er_) << "Unknown PBFT state " << shared_state_->state_;
      assert(false);
  }
  LOG(log_tr_) << "next step time(ms): " << shared_state_->next_step_time_ms_ << ", step " << step_;
}

bool PbftManager::stateOperations_() {
  pushSyncedPbftBlocksIntoChain();

  checkPreviousRoundNextVotedValueChange_();

  shared_state_->now_ = std::chrono::system_clock::now();
  shared_state_->duration_ = shared_state_->now_ - shared_state_->round_clock_initial_datetime_;
  shared_state_->elapsed_time_in_round_ms_ =
      std::chrono::duration_cast<std::chrono::milliseconds>(shared_state_->duration_).count();

  auto round = getPbftRound();
  LOG(log_tr_) << "PBFT current round is " << round << ", step is " << step_;

  // Periodically verify unverified votes
  node_->vote_mgr_->verifyVotes(round, [this](auto const &v) {
    try {
      v->validate(dposEligibleVoteCount_(v->getVoterAddr()), getDposTotalVotesCount(), getThreshold(v->getType()));
    } catch (const std::logic_error &e) {
      LOG(log_wr_) << e.what();
      return false;
    }

    return true;
  });

  // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR CURRENT
  // ROUND.  IF WE HAVE THEN WE EXECUTE THE BLOCK
  // ONLY CHECK IF HAVE *NOT* YET EXECUTED THIS ROUND...
  if (shared_state_->state_ == certify && !have_executed_this_round_) {
    auto voted_block_hash_with_cert_votes = node_->vote_mgr_->getVotesBundleByRoundAndStep(round, 3, TWO_T_PLUS_ONE);
    if (voted_block_hash_with_cert_votes) {
      LOG(log_dg_) << "PBFT block " << voted_block_hash_with_cert_votes->voted_block_hash << " has enough certed votes";
      // put pbft block into chain
      const auto vote_size = voted_block_hash_with_cert_votes->votes.size();
      if (pushCertVotedPbftBlockIntoChain_(voted_block_hash_with_cert_votes->voted_block_hash,
                                           std::move(voted_block_hash_with_cert_votes->votes))) {
        node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedInRound, true);
        have_executed_this_round_ = true;
        LOG(log_nf_) << "Write " << vote_size << " cert votes ... in round " << round;

        shared_state_->duration_ = std::chrono::system_clock::now() - shared_state_->now_;
        auto execute_trxs_in_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(shared_state_->duration_).count();
        LOG(log_dg_) << "Pushing PBFT block and Execution spent " << execute_trxs_in_ms << " ms. in round " << round;
        // Restart while loop
        return true;
      }
    }
  }

  return resetRound_();
}

bool PbftManager::updateSoftVotedBlockForThisRound_() {
  if (shared_state_->soft_voted_block_for_this_round_) {
    // Have enough soft votes for current round already
    return true;
  }

  auto round = getPbftRound();
  const auto voted_block_hash_with_soft_votes =
      node_->vote_mgr_->getVotesBundleByRoundAndStep(round, 2, TWO_T_PLUS_ONE);
  if (voted_block_hash_with_soft_votes) {
    // Have enough soft votes for a voted value
    auto batch = node_->db_->createWriteBatch();
    node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound,
                                            voted_block_hash_with_soft_votes->voted_block_hash, batch);
    node_->db_->addSoftVotesToBatch(round, voted_block_hash_with_soft_votes->votes, batch);
    node_->db_->commitWriteBatch(batch);

    shared_state_->soft_voted_block_for_this_round_ = voted_block_hash_with_soft_votes->voted_block_hash;

    return true;
  }

  return false;
}

void PbftManager::initializeVotedValueTimeouts_() {
  shared_state_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
  shared_state_->time_began_waiting_soft_voted_block_ = std::chrono::system_clock::now();

  // Concern: Requires that shared_state_->soft_voted_block_for_this_round_ already be initialized from db
  if (shared_state_->soft_voted_block_for_this_round_) {
    shared_state_->last_soft_voted_value_ = shared_state_->soft_voted_block_for_this_round_;
  }
}

void PbftManager::checkPreviousRoundNextVotedValueChange_() {
  auto previous_round_next_voted_value = node_->next_votes_manager_->getVotedValue();
  auto previous_round_next_voted_null_block_hash = node_->next_votes_manager_->haveEnoughVotesForNullBlockHash();

  if (previous_round_next_voted_value != shared_state_->previous_round_next_voted_value_) {
    shared_state_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    shared_state_->previous_round_next_voted_value_ = previous_round_next_voted_value;
  } else if (previous_round_next_voted_null_block_hash != previous_round_next_voted_null_block_hash_) {
    shared_state_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    previous_round_next_voted_null_block_hash_ = previous_round_next_voted_null_block_hash;
  }
}

std::shared_ptr<Vote> PbftManager::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type, uint64_t round,
                                                size_t step) {
  // sortition proof
  VrfPbftSortition vrf_sortition(node_->vrf_sk_, {type, round, step});
  return std::make_shared<Vote>(node_->node_sk_, std::move(vrf_sortition), blockhash);
}

uint64_t PbftManager::getThreshold(PbftVoteTypes vote_type) const {
  switch (vote_type) {
    case propose_vote_type:
      return std::min<uint64_t>(config_.number_of_proposers, getDposTotalVotesCount());
    case soft_vote_type:
    case cert_vote_type:
    case next_vote_type:
    default:
      return sortition_threshold_;
  }
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

h256 PbftManager::getProposal(const std::shared_ptr<Vote> &vote) const {
  auto lowest_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), 1);
  for (uint64_t i = 2; i <= vote->getWeight(); ++i) {
    auto tmp_hash = getVoterIndexHash(vote->getCredential(), vote->getVoter(), i);
    if (lowest_hash > tmp_hash) {
      lowest_hash = tmp_hash;
    }
  }
  return lowest_hash;
}

blk_hash_t PbftManager::identifyLeaderBlock_() {
  auto round = getPbftRound();
  LOG(log_dg_) << "Into identify leader block, in round " << round;

  // Get all proposal votes in the round
  auto votes = node_->vote_mgr_->getProposalVotes(round);

  // Each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<h256, blk_hash_t>> leader_candidates;

  for (auto const &v : votes) {
    if (v->getRound() != round) {
      LOG(log_er_) << "Vote round is not same with current round " << round << ". Vote " << v;
      continue;
    }
    if (v->getStep() != 1) {
      LOG(log_er_) << "Vote step is not 1. Vote " << v;
      continue;
    }
    if (v->getType() != propose_vote_type) {
      LOG(log_er_) << "Vote type is not propose vote type. Vote " << v;
      continue;
    }

    const auto proposed_block_hash = v->getBlockHash();
    if (proposed_block_hash == kNullBlockHash) {
      LOG(log_er_) << "Propose block hash should not be NULL. Vote " << v;
      continue;
    }
    if (node_->pbft_chain_->findPbftBlockInChain(proposed_block_hash)) {
      continue;
    }
    // Make sure we don't keep soft voting for soft value we want to give up...
    if (proposed_block_hash == shared_state_->last_soft_voted_value_ && giveUpSoftVotedBlock_()) {
      continue;
    }

    leader_candidates.emplace_back(std::make_pair(getProposal(v), proposed_block_hash));
  }

  if (leader_candidates.empty()) {
    // no eligible leader
    return kNullBlockHash;
  }

  const auto leader = *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                                        [](const auto &i, const auto &j) { return i.first < j.first; });
  return leader.second;
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return getPbftRound() == pbft_round_last_requested_sync_ && step_ == pbft_step_last_requested_sync_;
}

void PbftManager::syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash) {
  if (stopped_) {
    return;
  }
  if (auto net = node_->network_.lock()) {
    if (periodDataQueueSize()) {
      LOG(log_tr_) << "PBFT synced queue is still processing so skip syncing. Synced queue size "
                   << periodDataQueueSize();

      return;
    }

    if (!is_syncing_() && !syncRequestedAlreadyThisStep_()) {
      auto round = getPbftRound();

      switch (reason) {
        case missing_dag_blk:
          LOG(log_nf_) << "DAG blocks have not synced yet, anchor block " << relevant_blk_hash
                       << " not present in DAG.";

          break;
        case invalid_cert_voted_block:
          // Get partition, need send request to get missing pbft blocks from peers
          LOG(log_nf_) << "Cert voted block " << relevant_blk_hash
                       << " is invalid, we must be out of sync with pbft chain.";
          break;
        case invalid_soft_voted_block:
          // TODO: Address CONCERN of should we sync here?  Any malicious player can propose an invalid soft voted
          // block... Honest nodes will soft vote for any malicious block before receiving it and verifying it.
          LOG(log_nf_) << "Soft voted block for this round appears to be invalid, perhaps node out of sync";
          break;
        case exceeded_max_steps:
          LOG(log_nf_) << "Suspect consensus is partitioned, reached step " << step_ << " in round " << round
                       << " without advancing.";
          // We want to force sycning the DAG...
          break;
        default:
          LOG(log_er_) << "Unknown PBFT sync request reason " << reason;
          assert(false);

          LOG(log_nf_) << "Restarting sync in round " << round << ", step " << step_;
      }

      // TODO: Is there any need to sync here???
      // If we detect some stalled situation, a better step would be to disconnect/reconnect to nodes to try to get
      // network unstuck since reconnecting both shuffles the nodes and invokes pbft/dag syncing if needed
      // net->restartSyncingPbft();

      pbft_round_last_requested_sync_ = round;
      pbft_step_last_requested_sync_ = step_;
    }
  }
}

bool PbftManager::compareBlocksAndRewardVotes_(const blk_hash_t &pbft_block_hash) {
  auto pbft_block = getUnfinalizedBlock_(pbft_block_hash);
  if (!pbft_block) {
    return false;
  }

  return compareBlocksAndRewardVotes_(std::move(pbft_block)).second;
}

std::pair<vec_blk_t, bool> PbftManager::compareBlocksAndRewardVotes_(std::shared_ptr<PbftBlock> pbft_block) {
  vec_blk_t dag_blocks_order;
  const auto proposal_block_hash = pbft_block->getBlockHash();
  // If cert period data is already populated with this pbft block, no need to do verification again
  if (period_data_.pbft_blk && period_data_.pbft_blk->getBlockHash() == proposal_block_hash) {
    dag_blocks_order.reserve(period_data_.dag_blocks.size());
    std::transform(period_data_.dag_blocks.begin(), period_data_.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                   [](const DagBlock &dag_block) { return dag_block.getHash(); });
    return std::make_pair(std::move(dag_blocks_order), true);
  }

  auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
  if (anchor_hash == kNullBlockHash) {
    period_data_.clear();
    period_data_.pbft_blk = std::move(pbft_block);
    return std::make_pair(std::move(dag_blocks_order), true);
  }

  dag_blocks_order = node_->dag_mgr_->getDagBlockOrder(anchor_hash, pbft_block->getPeriod());
  if (dag_blocks_order.empty()) {
    syncPbftChainFromPeers_(missing_dag_blk, anchor_hash);
    return std::make_pair(std::move(dag_blocks_order), false);
  }

  std::unordered_set<trx_hash_t> trx_set;
  std::vector<trx_hash_t> transactions_to_query;
  period_data_.clear();
  period_data_.dag_blocks.reserve(dag_blocks_order.size());
  for (auto const &dag_blk_hash : dag_blocks_order) {
    auto dag_block = node_->dag_blk_mgr_->getDagBlock(dag_blk_hash);
    assert(dag_block);
    for (auto const &trx_hash : dag_block->getTrxs()) {
      if (trx_set.insert(trx_hash).second) {
        transactions_to_query.emplace_back(trx_hash);
      }
    }
    period_data_.dag_blocks.emplace_back(std::move(*dag_block));
  }
  std::vector<trx_hash_t> non_finalized_transactions;
  auto trx_finalized = node_->db_->transactionsFinalized(transactions_to_query);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(transactions_to_query[i]);
    }
  }

  const auto transactions = node_->trx_mgr_->getNonfinalizedTrx(non_finalized_transactions, true /*sorted*/);
  non_finalized_transactions.clear();
  period_data_.transactions.reserve(transactions.size());
  for (const auto &trx : transactions) {
    non_finalized_transactions.push_back(trx->getHash());
    period_data_.transactions.push_back(trx);
  }

  auto calculated_order_hash = pbft::calculateOrderHash(dag_blocks_order, non_finalized_transactions);
  if (calculated_order_hash != pbft_block->getOrderHash()) {
    LOG(log_er_) << "Order hash incorrect. Pbft block: " << proposal_block_hash
                 << ". Order hash: " << pbft_block->getOrderHash() << " . Calculated hash:" << calculated_order_hash
                 << ". Dag order: " << dag_blocks_order << ". Trx order: " << non_finalized_transactions;
    dag_blocks_order.clear();
    return std::make_pair(std::move(dag_blocks_order), false);
  }

  auto last_pbft_block_hash = node_->pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_pbft_block = node_->pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);

    std::vector<blk_hash_t> ghost;
    node_->dag_mgr_->getGhostPath(prev_pbft_block.getPivotDagBlockHash(), ghost);
    if (ghost.size() > 1 && anchor_hash != ghost[1]) {
      if (!checkBlockWeight(period_data_)) {
        LOG(log_er_) << "PBFT block " << proposal_block_hash << " is overweighted";
        return std::make_pair(std::move(dag_blocks_order), false);
      }
    }
  }

  // Check reward votes
  if (!node_->vote_mgr_->checkRewardVotes(pbft_block)) {
    LOG(log_er_) << "Failed verifying reward votes for proposed PBFT block " << proposal_block_hash;
    return {{}, false};
  }

  period_data_.pbft_blk = std::move(pbft_block);

  return std::make_pair(std::move(dag_blocks_order), true);
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(taraxa::blk_hash_t const &cert_voted_block_hash,
                                                   std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes) {
  auto pbft_block = getUnfinalizedBlock_(cert_voted_block_hash);
  if (!pbft_block) {
    LOG(log_nf_) << "Can not find the cert voted block hash " << cert_voted_block_hash << " in both pbft queue and DB";
    return false;
  }

  if (!node_->pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
    syncPbftChainFromPeers_(invalid_cert_voted_block, cert_voted_block_hash);
    return false;
  }

  auto [dag_blocks_order, ok] = compareBlocksAndRewardVotes_(pbft_block);
  if (!ok) {
    LOG(log_nf_) << "Failed compare DAG blocks or reward votes with PBFT block " << cert_voted_block_hash;
    return false;
  }

  period_data_.previous_block_cert_votes = getRewardVotesInBlock(period_data_.pbft_blk->getRewardVotes());

  if (!pushPbftBlock_(std::move(period_data_), std::move(current_round_cert_votes), std::move(dag_blocks_order))) {
    LOG(log_er_) << "Failed push PBFT block " << pbft_block->getBlockHash() << " into chain";
    return false;
  }

  // cleanup PBFT unverified blocks table
  node_->pbft_chain_->cleanupUnverifiedPbftBlocks(*pbft_block);

  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain() {
  if (auto net = node_->network_.lock()) {
    auto round = getPbftRound();
    while (periodDataQueueSize() > 0) {
      auto period_data_opt = processPeriodData();
      if (!period_data_opt) continue;
      auto period_data = std::move(period_data_opt.value());
      const auto period = period_data.first.pbft_blk->getPeriod();
      auto pbft_block_hash = period_data.first.pbft_blk->getBlockHash();
      LOG(log_nf_) << "Pick pbft block " << pbft_block_hash << " from synced queue in round " << round;

      if (pushPbftBlock_(std::move(period_data.first), std::move(period_data.second))) {
        LOG(log_nf_) << node_addr_ << " push synced PBFT block " << pbft_block_hash << " in round " << round;
      } else {
        LOG(log_er_) << "Failed push PBFT block " << pbft_block_hash << " into chain";
        break;
      }

      net->setSyncStatePeriod(period);

      if (executed_pbft_block_) {
        node_->vote_mgr_->removeVerifiedVotes();
        updateDposState_();
        // update sortition_threshold and TWO_T_PLUS_ONE
        updateTwoTPlusOneAndThreshold_();
        node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
        executed_pbft_block_ = false;
      }
    }
  }
}

void PbftManager::finalize_(PeriodData &&period_data, std::vector<h256> &&finalized_dag_blk_hashes, bool sync) {
  const auto anchor = period_data.pbft_blk->getPivotDagBlockHash();

  auto result = node_->final_chain_->finalize(
      std::move(period_data), std::move(finalized_dag_blk_hashes),
      [this, weak_ptr = weak_from_this(), anchor_hash = anchor, period = period_data.pbft_blk->getPeriod()](
          auto const &, auto &batch) {
        // Update proposal period DAG levels map
        auto ptr = weak_ptr.lock();
        if (!ptr) return;  // it was destroyed

        if (!anchor_hash) {
          // Null anchor don't update proposal period DAG levels map
          return;
        }

        auto anchor = node_->dag_blk_mgr_->getDagBlock(anchor_hash);
        if (!anchor) {
          LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor_hash << " in DB.";
          assert(false);
        }

        node_->db_->addProposalPeriodDagLevelsMapToBatch(anchor->getLevel() + max_levels_per_period_, period, batch);
      });

  if (sync) {
    result.wait();
  }
}

bool PbftManager::pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes,
                                 vec_blk_t &&dag_blocks_order) {
  auto const &pbft_block_hash = period_data.pbft_blk->getBlockHash();
  if (node_->db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_nf_) << "PBFT block: " << pbft_block_hash << " in DB already.";
    if (shared_state_->last_cert_voted_value_ == pbft_block_hash) {
      LOG(log_er_) << "Last cert voted value should be kNullBlockHash. Block hash "
                   << shared_state_->last_cert_voted_value_ << " has been pushed into chain already";
      assert(false);
    }
    return false;
  }

  auto pbft_period = period_data.pbft_blk->getPeriod();
  auto null_anchor = period_data.pbft_blk->getPivotDagBlockHash() == kNullBlockHash;

  auto batch = node_->db_->createWriteBatch();

  LOG(log_nf_) << "Storing cert votes of pbft blk " << pbft_block_hash;
  LOG(log_dg_) << "Stored following cert votes:\n" << cert_votes;
  // Update PBFT chain head block
  node_->db_->addPbftHeadToBatch(node_->pbft_chain_->getHeadHash(),
                                 node_->pbft_chain_->getJsonStrForBlock(pbft_block_hash, null_anchor), batch);

  if (dag_blocks_order.empty()) {
    dag_blocks_order.reserve(period_data.dag_blocks.size());
    std::transform(period_data.dag_blocks.begin(), period_data.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                   [](const DagBlock &dag_block) { return dag_block.getHash(); });
  }

  node_->db_->savePeriodData(period_data, batch);
  node_->db_->addLastBlockCertVotesToBatch(cert_votes, batch);

  // Reset last cert voted value to kNullBlockHash
  node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::LastCertVotedValue, kNullBlockHash, batch);

  // pass pbft with dag blocks and transactions to adjust difficulty
  if (period_data.pbft_blk->getPivotDagBlockHash() != kNullBlockHash) {
    node_->dag_blk_mgr_->sortitionParamsManager().pbftBlockPushed(
        period_data, batch, node_->pbft_chain_->getPbftChainSizeExcludingEmptyPbftBlocks() + 1);
  }
  {
    // This makes sure that no DAG block or transaction can be added or change state in transaction and dag manager when
    // finalizing pbft block with dag blocks and transactions
    std::unique_lock dag_lock(node_->dag_mgr_->getDagMutex());
    std::unique_lock trx_lock(node_->trx_mgr_->getTransactionsMutex());

    // Commit DB
    node_->db_->commitWriteBatch(batch);

    // Set DAG blocks period
    auto const &anchor_hash = period_data.pbft_blk->getPivotDagBlockHash();
    node_->dag_mgr_->setDagBlockOrder(anchor_hash, pbft_period, dag_blocks_order);

    node_->trx_mgr_->updateFinalizedTransactionsStatus(period_data);

    // update PBFT chain size
    node_->pbft_chain_->updatePbftChain(pbft_block_hash, null_anchor);
  }

  node_->vote_mgr_->replaceRewardVotes(std::move(cert_votes));

  shared_state_->last_cert_voted_value_ = kNullBlockHash;

  LOG(log_nf_) << "Pushed new PBFT block " << pbft_block_hash << " into chain. Period: " << pbft_period
               << ", round: " << getPbftRound();

  finalize_(std::move(period_data), std::move(dag_blocks_order));

  // Reset proposed PBFT block hash to NULL for next period PBFT block proposal
  proposed_block_hash_ = kNullBlockHash;
  node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  executed_pbft_block_ = true;
  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  // Update 2t+1 and threshold
  auto dpos_total_votes_count = getDposTotalVotesCount();
  sortition_threshold_ = std::min<size_t>(config_.committee_size, dpos_total_votes_count);
  TWO_T_PLUS_ONE = sortition_threshold_ * 2 / 3 + 1;
  LOG(log_nf_) << "Committee size " << config_.committee_size << ", DPOS total votes count " << dpos_total_votes_count
               << ". Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold " << sortition_threshold_;
}

bool PbftManager::giveUpSoftVotedBlock_() {
  if (shared_state_->last_soft_voted_value_ == kNullBlockHash) return false;

  auto now = std::chrono::system_clock::now();
  auto soft_voted_block_wait_duration = now - shared_state_->time_began_waiting_soft_voted_block_;
  unsigned long elapsed_wait_soft_voted_block_in_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(soft_voted_block_wait_duration).count();

  auto pbft_block = getUnfinalizedBlock_(shared_state_->previous_round_next_voted_value_);
  if (pbft_block) {
    // Have a block, but is it valid?
    if (!node_->pbft_chain_->checkPbftBlockValidation(*pbft_block)) {
      // Received the block, but not valid
      return true;
    }
  }

  if (elapsed_wait_soft_voted_block_in_ms > max_wait_for_soft_voted_block_steps_ms_) {
    LOG(log_dg_) << "Have been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << shared_state_->last_soft_voted_value_ << ", giving up on this value.";
    return true;
  } else {
    LOG(log_tr_) << "Have only been waiting " << elapsed_wait_soft_voted_block_in_ms << "ms for soft voted block "
                 << shared_state_->last_soft_voted_value_ << "(after " << max_wait_for_soft_voted_block_steps_ms_
                 << "ms will give up on this value)";
  }

  return false;
}

std::shared_ptr<PbftBlock> PbftManager::getUnfinalizedBlock_(blk_hash_t const &block_hash) {
  auto block = node_->pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!block) {
    block = node_->db_->getPbftCertVotedBlock(block_hash);
    if (block) {
      // PBFT unverified queue empty after node reboot, read from DB pushing back in unverified queue
      node_->pbft_chain_->pushUnverifiedPbftBlock(block);
    }
  }

  return block;
}

bool PbftManager::is_syncing_() {
  if (auto net = node_->network_.lock()) {
    return net->pbft_syncing();
  }
  return false;
}

uint64_t PbftManager::pbftSyncingPeriod() const {
  return std::max(sync_queue_.getPeriod(), node_->pbft_chain_->getPbftChainSize());
}

std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>> PbftManager::processPeriodData() {
  auto [period_data, cert_votes, node_id] = sync_queue_.pop();
  auto pbft_block_hash = period_data.pbft_blk->getBlockHash();
  LOG(log_nf_) << "Pop pbft block " << pbft_block_hash << " from synced queue";

  if (node_->pbft_chain_->findPbftBlockInChain(pbft_block_hash)) {
    LOG(log_dg_) << "PBFT block " << pbft_block_hash << " already present in chain.";
    return std::nullopt;
  }

  auto net = node_->network_.lock();
  assert(net);  // Should never happen

  auto last_pbft_block_hash = node_->pbft_chain_->getLastPbftBlockHash();

  // Check previous hash matches
  if (period_data.pbft_blk->getPrevBlockHash() != last_pbft_block_hash) {
    auto last_pbft_block = node_->pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    if (period_data.pbft_blk->getPeriod() <= last_pbft_block.getPeriod()) {
      // Old block in the sync queue
      return std::nullopt;
    }

    LOG(log_er_) << "Invalid PBFT block " << pbft_block_hash
                 << "; prevHash: " << period_data.pbft_blk->getPrevBlockHash() << " from peer " << node_id.abridged()
                 << " received, stop syncing.";
    sync_queue_.clear();
    // Handle malicious peer on network level
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  // Check reward votes
  if (!node_->vote_mgr_->checkRewardVotes(period_data.pbft_blk)) {
    LOG(log_er_) << "Failed verifying reward votes. Disconnect malicious peer " << node_id.abridged();
    sync_queue_.clear();
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  // Check cert votes validation
  try {
    period_data.hasEnoughValidCertVotes(cert_votes, getDposTotalVotesCount(), getThreshold(cert_vote_type),
                                        getTwoTPlusOne(),
                                        [this](auto const &addr) { return dposEligibleVoteCount_(addr); });
  } catch (const std::logic_error &e) {
    // Failed cert votes validation, flush synced PBFT queue and set since
    // next block validation depends on the current one
    LOG(log_er_) << e.what();  // log exception text from hasEnoughValidCertVotes
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash
                 << " doesn't have enough valid cert votes. Clear synced PBFT blocks! DPOS total votes count: "
                 << getDposTotalVotesCount();
    sync_queue_.clear();
    net->getSpecificHandler<network::tarcap::PbftSyncPacketHandler>()->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }

  return std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>>(
      {std::move(period_data), std::move(cert_votes)});
}

blk_hash_t PbftManager::lastPbftBlockHashFromQueueOrChain() {
  auto pbft_block = sync_queue_.lastPbftBlock();
  if (pbft_block) return pbft_block->getBlockHash();
  return node_->pbft_chain_->getLastPbftBlockHash();
}

bool PbftManager::periodDataQueueEmpty() const { return sync_queue_.empty(); }

void PbftManager::periodDataQueuePush(PeriodData &&period_data, dev::p2p::NodeID const &node_id,
                                      std::vector<std::shared_ptr<Vote>> &&current_block_cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();
  if (!sync_queue_.push(std::move(period_data), node_id, node_->pbft_chain_->getPbftChainSize(),
                        std::move(current_block_cert_votes))) {
    LOG(log_er_) << "Trying to push period data with " << period << " period, but current period is "
                 << sync_queue_.getPeriod();
  }
}

size_t PbftManager::periodDataQueueSize() const { return sync_queue_.size(); }

std::unordered_map<trx_hash_t, u256> getAllTrxEstimations(const PeriodData &period_data) {
  std::unordered_map<trx_hash_t, u256> result;
  for (const auto &dag_block : period_data.dag_blocks) {
    const auto &transactions = dag_block.getTrxs();
    const auto &estimations = dag_block.getTrxsGasEstimations();
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      result.emplace(transactions[i], estimations[i]);
    }
  }
  return result;
}

bool PbftManager::checkBlockWeight(const PeriodData &period_data) const {
  const auto &trx_estimations = getAllTrxEstimations(period_data);

  u256 total_weight = 0;
  for (const auto &tx : period_data.transactions) {
    total_weight += trx_estimations.at(tx->getHash());
  }
  if (total_weight > config_.gas_limit) {
    return false;
  }
  return true;
}

blk_hash_t PbftManager::getLastPbftBlockHash() { return node_->pbft_chain_->getLastPbftBlockHash(); }

std::vector<std::shared_ptr<Vote>> PbftManager::getRewardVotesInBlock(
    const std::vector<vote_hash_t> &reward_votes_hashes) {
  std::unordered_set<vote_hash_t> reward_votes_hashes_set;
  for (const auto &v : reward_votes_hashes) reward_votes_hashes_set.insert(v);

  auto reward_votes = node_->vote_mgr_->getRewardVotes();
  for (auto it = reward_votes.begin(); it != reward_votes.end();) {
    if (reward_votes_hashes_set.contains((*it)->getHash())) {
      ++it;
    } else {
      it = reward_votes.erase(it);
    }
  }

  return reward_votes;
}

}  // namespace taraxa
