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

  node_ = std::make_shared<NodeFace>(std::move(node_addr), std::move(node_sk), std::move(vrf_sk), conf, std::move(db),
                                     std::move(next_votes_mgr), std::move(pbft_chain), std::move(vote_mgr),
                                     std::move(dag_mgr), std::move(dag_blk_mgr), std::move(trx_mgr),
                                     std::move(final_chain));
}

PbftManager::~PbftManager() { stop(); }

void PbftManager::setNetwork(std::weak_ptr<Network> network) { node_->network_ = std::move(network); }

void PbftManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  node_->pbft_manager_ = weak_from_this();

  daemon_ = std::make_unique<std::thread>([this]() { run(); });

  LOG(log_dg_) << "PBFT daemon initiated ...";
}

void PbftManager::stop() {
  stopped_ = true;
  {
    std::unique_lock<std::mutex> lock(stop_mtx_);
    stop_cv_.notify_all();
  }
  if (daemon_ && daemon_->joinable()) {
    daemon_->join();
  }
  if (round_) {
    round_->finish();
    round_.reset();
  }

  LOG(log_er_) << "PBFT daemon terminated ...";
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
      LOG(log_er_) << "DB corrupted - PBFT block " << period_data.pbft_blk->getBlockHash() << " has different period "
                   << period_data.pbft_blk->getPeriod() << " in block data than in block order db: " << period;
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
    std::this_thread::sleep_for(std::chrono::milliseconds(round_->getLambda() / 4));
  }
}

uint64_t PbftManager::getPbftRound() const {
  if (!round_) {
    return 1;
  }
  return round_->getId();
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
      // Sleep one PBFT configured lambda time(without backoff)
      const auto polling_interval = 100;
      thisThreadSleepForMilliSeconds(polling_interval);
    }
  } while (!stopped_);

  LOG(log_nf_) << "DPOS total votes count is " << dpos_votes_count_ << " for period " << dpos_period_ << ". Account "
               << node_addr_ << " has " << weighted_votes_count_ << " weighted votes";
  updateTwoTPlusOneAndThreshold_();
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

void PbftManager::moveToRound_(uint64_t round, std::optional<uint64_t> step) {
  LOG(log_nf_) << "Advancing to pbft round " << round << ", step 1, and resetting clock.";
  if (round_) {
    round_->finish();
    updateDposState_();
  }
  round_ = Round::make(round, step, node_);
}

void PbftManager::initialize() {
  // Initialize PBFT state

  auto round = node_->db_->getPbftMgrField(PbftMgrRoundStep::PbftRound);
  auto step = node_->db_->getPbftMgrField(PbftMgrRoundStep::PbftStep);
  round_ = std::make_shared<Round>(round, node_);

  max_wait_for_soft_voted_block_steps_ms_ = MAX_WAIT_FOR_SOFT_VOTED_BLOCK_STEPS * 2 * round_->getLambda();

  if (round > 1) {
    // Get next votes for previous round from DB
    auto next_votes_in_previous_round = node_->db_->getNextVotes(round - 1);
    if (next_votes_in_previous_round.empty()) {
      LOG(log_er_) << "Cannot get any next votes in previous round " << round - 1 << ". Currrent round " << round
                   << " step " << step;
      // assert(false);
    }
    auto previous_round_2t_plus_1 = node_->db_->getPbft2TPlus1(round - 1);
    if (previous_round_2t_plus_1 == 0) {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in previous round " << round - 1 << ". Current round " << round << " step "
                   << step;
      assert(false);
    }
    node_->next_votes_manager_->updateNextVotes(next_votes_in_previous_round, previous_round_2t_plus_1);
  }

  round_->previous_round_next_voted_value_ = node_->next_votes_manager_->getVotedValue();
  round_->previous_round_next_voted_null_block_hash_ = node_->next_votes_manager_->haveEnoughVotesForNullBlockHash();

  LOG(log_nf_) << "Node initialize at round " << round << " step " << step
               << ". Previous round has enough next votes for kNullBlockHash: " << std::boolalpha
               << node_->next_votes_manager_->haveEnoughVotesForNullBlockHash() << ", voted value "
               << round_->previous_round_next_voted_value_ << ", next votes size in previous round is "
               << node_->next_votes_manager_->getNextVotesWeight();

  // Initial last sync request
  pbft_round_last_requested_sync_ = 0;
  pbft_step_last_requested_sync_ = 0;

  auto own_starting_value = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound);
  if (own_starting_value) {
    // From DB
    round_->own_starting_value_for_round_ = *own_starting_value;
  }

  auto soft_voted_block_hash = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound);
  if (soft_voted_block_hash) {
    // From DB
    round_->soft_voted_block_ = *soft_voted_block_hash;
    for (const auto &vote : node_->db_->getSoftVotes(round)) {
      if (vote->getBlockHash() != *soft_voted_block_hash) {
        LOG(log_er_) << "The soft vote should have voting value " << *soft_voted_block_hash << ". Vote" << vote;
        continue;
      }
      node_->vote_mgr_->addVerifiedVote(vote);
    }
  } else {
    // Default value
    round_->soft_voted_block_ = kNullBlockHash;
  }

  // Used for detecting timeout due to malicious node
  // failing to gossip PBFT block or DAG blocks
  // Requires that round_->soft_voted_block_ already be initialized from db
  initializeVotedValueTimeouts_();

  auto last_cert_voted_value = node_->db_->getPbftMgrVotedValue(PbftMgrVotedValue::LastCertVotedValue);
  if (last_cert_voted_value) {
    round_->last_cert_voted_value_ = *last_cert_voted_value;
    LOG(log_nf_) << "Initialize last cert voted value " << round_->last_cert_voted_value_;
  }

  round_->executed_pbft_block_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::ExecutedBlock);
  round_->block_certified_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::ExecutedInRound);
  round_->next_voted_soft_value_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::NextVotedSoftValue);
  round_->next_voted_null_block_hash_ = node_->db_->getPbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash);

  round_->start(step);

  updateDposState_();
}

bool PbftManager::stateOperations_() {
  pushSyncedPbftBlocksIntoChain();

  checkPreviousRoundNextVotedValueChange_();

  auto round_id = getPbftRound();
  LOG(log_tr_) << "PBFT current round is " << round_id << ", step is " << round_->getStepId();

  // Periodically verify unverified votes
  node_->vote_mgr_->verifyVotes(round_id, [this](auto const &v) {
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
  if (round_->getStepType() == StepType::certify && !round_->block_certified_) {
    auto time_before = std::chrono::system_clock::now();
    auto voted_block_hash_with_cert_votes = node_->vote_mgr_->getVotesBundleByRoundAndStep(round_id, 3, TWO_T_PLUS_ONE);
    if (voted_block_hash_with_cert_votes) {
      LOG(log_dg_) << "PBFT block " << voted_block_hash_with_cert_votes->voted_block_hash << " has enough certed votes";
      // put pbft block into chain
      const auto vote_size = voted_block_hash_with_cert_votes->votes.size();
      if (pushCertVotedPbftBlockIntoChain_(voted_block_hash_with_cert_votes->voted_block_hash,
                                           std::move(voted_block_hash_with_cert_votes->votes))) {
        node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedInRound, true);
        round_->block_certified_ = true;
        LOG(log_nf_) << "Write " << vote_size << " cert votes ... in round " << round_id;

        auto trxs_exec_time = std::chrono::system_clock::now() - time_before;
        auto trxs_exec_time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(trxs_exec_time).count();
        LOG(log_dg_) << "Pushing PBFT block and Execution spent " << trxs_exec_time_in_ms << " ms. in round "
                     << round_id;
        // Restart while loop
        return true;
      }
    }
  }

  // Check if we have 2t+1 next_votes to move to next round
  auto next_votes_round = node_->vote_mgr_->roundDeterminedFromVotes(TWO_T_PLUS_ONE);

  if (!next_votes_round || next_votes_round <= round_id) {
    return false;
  }

  moveToRound_(next_votes_round.value());
  return true;
}

bool PbftManager::updateSoftVotedBlockForThisRound_() {
  if (round_->soft_voted_block_) {
    // Have enough soft votes for current round already
    return true;
  }

  auto round_id = getPbftRound();
  const auto voted_block_hash_with_soft_votes =
      node_->vote_mgr_->getVotesBundleByRoundAndStep(round_id, 2, TWO_T_PLUS_ONE);
  if (voted_block_hash_with_soft_votes) {
    // Have enough soft votes for a voted value
    auto batch = node_->db_->createWriteBatch();
    node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound,
                                            voted_block_hash_with_soft_votes->voted_block_hash, batch);
    node_->db_->addSoftVotesToBatch(round_id, voted_block_hash_with_soft_votes->votes, batch);
    node_->db_->commitWriteBatch(batch);

    round_->soft_voted_block_ = voted_block_hash_with_soft_votes->voted_block_hash;

    return true;
  }

  return false;
}

void PbftManager::initializeVotedValueTimeouts_() {
  round_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
  round_->time_began_waiting_soft_voted_block_ = std::chrono::system_clock::now();

  // Concern: Requires that round_->soft_voted_block_ already be initialized from db
  if (round_->soft_voted_block_) {
    last_soft_voted_value_ = round_->soft_voted_block_;
  }
}

void PbftManager::checkPreviousRoundNextVotedValueChange_() {
  auto previous_round_next_voted_value = node_->next_votes_manager_->getVotedValue();
  auto previous_round_next_voted_null_block_hash = node_->next_votes_manager_->haveEnoughVotesForNullBlockHash();

  if (previous_round_next_voted_value != round_->previous_round_next_voted_value_) {
    round_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    round_->previous_round_next_voted_value_ = previous_round_next_voted_value;
  } else if (previous_round_next_voted_null_block_hash != round_->previous_round_next_voted_null_block_hash_) {
    round_->time_began_waiting_next_voted_block_ = std::chrono::system_clock::now();
    round_->previous_round_next_voted_null_block_hash_ = previous_round_next_voted_null_block_hash;
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

// bool PbftManager::syncRequestedAlreadyThisStep_() const {
//   return getPbftRound() == pbft_round_last_requested_sync_ && round_->getStepId() ==
//   pbft_step_last_requested_sync_;
// }

// void PbftManager::syncPbftChainFromPeers_(PbftSyncRequestReason reason, taraxa::blk_hash_t const &relevant_blk_hash)
// {
//   if (stopped_) {
//     return;
//   }
//   if (auto net = node_->network_.lock()) {
//     if (periodDataQueueSize()) {
//       LOG(log_tr_) << "PBFT synced queue is still processing so skip syncing. Synced queue size "
//                    << periodDataQueueSize();

//       return;
//     }

//     if (!is_syncing_() && !syncRequestedAlreadyThisStep_()) {
//       auto round = getPbftRound();

//       switch (reason) {
//         case missing_dag_blk:
//           LOG(log_nf_) << "DAG blocks have not synced yet, anchor block " << relevant_blk_hash
//                        << " not present in DAG.";

//           break;
//         case invalid_cert_voted_block:
//           // Get partition, need send request to get missing pbft blocks from peers
//           LOG(log_nf_) << "Cert voted block " << relevant_blk_hash
//                        << " is invalid, we must be out of sync with pbft chain.";
//           break;
//         case invalid_soft_voted_block:
//           // TODO: Address CONCERN of should we sync here?  Any malicious player can propose an invalid soft voted
//           // block... Honest nodes will soft vote for any malicious block before receiving it and verifying it.
//           LOG(log_nf_) << "Soft voted block for this round appears to be invalid, perhaps node out of sync";
//           break;
//         case exceeded_max_steps:
//           LOG(log_nf_) << "Suspect consensus is partitioned, reached step " << round_->getStepId() << " in
//           round "
//           << round
//                        << " without advancing.";
//           // We want to force sycning the DAG...
//           break;
//         default:
//           LOG(log_er_) << "Unknown PBFT sync request reason " << reason;
//           assert(false);

//           LOG(log_nf_) << "Restarting sync in round " << round << ", step " << round_->getStepId();
//       }

//       // TODO: Is there any need to sync here???
//       // If we detect some stalled situation, a better step would be to disconnect/reconnect to nodes to try to get
//       // network unstuck since reconnecting both shuffles the nodes and invokes pbft/dag syncing if needed
//       // net->restartSyncingPbft();

//       pbft_round_last_requested_sync_ = round;
//       pbft_step_last_requested_sync_ = round_->getStepId();
//     }
//   }
// }

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
    // syncPbftChainFromPeers_(missing_dag_blk, anchor_hash);
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
    // syncPbftChainFromPeers_(invalid_cert_voted_block, cert_voted_block_hash);
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
    auto round_id = getPbftRound();
    while (periodDataQueueSize() > 0) {
      auto period_data_opt = processPeriodData();
      if (!period_data_opt) continue;
      auto period_data = std::move(period_data_opt.value());
      const auto period = period_data.first.pbft_blk->getPeriod();
      auto pbft_block_hash = period_data.first.pbft_blk->getBlockHash();
      LOG(log_nf_) << "Pick pbft block " << pbft_block_hash << " from synced queue in round " << round_id;

      if (pushPbftBlock_(std::move(period_data.first), std::move(period_data.second))) {
        LOG(log_nf_) << node_addr_ << " push synced PBFT block " << pbft_block_hash << " in round " << round_id;
      } else {
        LOG(log_er_) << "Failed push PBFT block " << pbft_block_hash << " into chain";
        break;
      }

      net->setSyncStatePeriod(period);

      if (round_->executed_pbft_block_) {
        node_->vote_mgr_->removeVerifiedVotes();
        updateDposState_();
        node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
        round_->executed_pbft_block_ = false;
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
    if (round_->last_cert_voted_value_ == pbft_block_hash) {
      LOG(log_er_) << "Last cert voted value should be kNullBlockHash. Block hash " << round_->last_cert_voted_value_
                   << " has been pushed into chain already";
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

  LOG(log_nf_) << "Pushed new PBFT block " << pbft_block_hash << " into chain. Period: " << pbft_period
               << ", round: " << getPbftRound();

  finalize_(std::move(period_data), std::move(dag_blocks_order));

  // Reset proposed PBFT block hash to NULL for next period PBFT block proposal
  node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  round_->executed_pbft_block_ = true;
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

// bool PbftManager::is_syncing_() {
//   if (auto net = node_->network_.lock()) {
//     return net->pbft_syncing();
//   }
//   return false;
// }

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
