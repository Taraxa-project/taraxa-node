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
#include "network/tarcap/packets_handlers/latest/pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"
#include "pbft/period_data.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {
using vrf_output_t = vrf_wrapper::vrf_output_t;

constexpr std::chrono::milliseconds kPollingIntervalMs{100};
constexpr PbftStep kMaxSteps{13};  // Need to be a odd number

PbftManager::PbftManager(const PbftConfig &conf, const blk_hash_t &dag_genesis_block_hash, addr_t node_addr,
                         std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr,
                         std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
                         secret_t node_sk)
    : db_(std::move(db)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      node_addr_(std::move(node_addr)),
      node_sk_(std::move(node_sk)),
      kMinLambda(conf.lambda_ms),
      dag_genesis_block_hash_(dag_genesis_block_hash),
      config_(conf),
      proposed_blocks_(db_) {
  LOG_OBJECTS_CREATE("PBFT_MGR");

  for (auto period = final_chain_->last_block_number() + 1, curr_period = pbft_chain_->getPbftChainSize();
       period <= curr_period; ++period) {
    auto period_raw = db_->getPeriodDataRaw(period);
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
      vote_mgr_->validateVote(v);
    }

    finalize_(std::move(period_data), db_->getFinalizedDagBlockHashesByPeriod(period), period == curr_period);
  }

  PbftPeriod start_period = 1;
  if (pbft_chain_->getPbftChainSize() > 2 * final_chain_->delegation_delay()) {
    start_period = pbft_chain_->getPbftChainSize() - 2 * final_chain_->delegation_delay();
  }
  for (PbftPeriod period = start_period; period <= pbft_chain_->getPbftChainSize(); period++) {
    auto period_raw = db_->getPeriodDataRaw(period);
    if (period_raw.size() == 0) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block in period " << period << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    PeriodData period_data(period_raw);
    trx_mgr_->initializeRecentlyFinalizedTransactions(period_data);
  }

  // Initialize PBFT status
  initialState();
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
    // we need to be sure we finalized at least block block with num lower by delegation_delay
    if (pbft_chain_->getPbftChainSize() <= final_chain_->last_block_number() + final_chain_->delegation_delay()) {
      break;
    }
    thisThreadSleepForMilliSeconds(kPollingIntervalMs.count());
  } while (!stopped_);
}

std::optional<uint64_t> PbftManager::getCurrentDposTotalVotesCount() const {
  try {
    return final_chain_->dpos_eligible_total_vote_count(pbft_chain_->getPbftChainSize());
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_wr_) << "Unable to get CurrentDposTotalVotesCount for period: " << pbft_chain_->getPbftChainSize()
                 << ". Period is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what();
  }

  return {};
}

std::optional<uint64_t> PbftManager::getCurrentNodeVotesCount() const {
  try {
    return final_chain_->dpos_eligible_vote_count(pbft_chain_->getPbftChainSize(), node_addr_);
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_wr_) << "Unable to get CurrentNodeVotesCount for period: " << pbft_chain_->getPbftChainSize()
                 << ". Period is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what();
  }

  return {};
}

void PbftManager::setPbftStep(PbftStep pbft_step) {
  db_->savePbftMgrField(PbftMgrField::Step, pbft_step);
  step_ = pbft_step;

  // Increase lambda only for odd steps (second finish steps) after node reached kMaxSteps steps
  if (step_ > kMaxSteps && step_ % 2) {
    const auto [round, period] = getPbftRoundAndPeriod();
    const auto network_next_voting_step = vote_mgr_->getNetworkTplusOneNextVotingStep(period, round);

    // Node is still >= kMaxSteps steps behind the rest (at least 1/3) of the network - keep lambda at the standard
    // value so node can catch up with the rest of the nodes

    // To get withing 1 round with the rest of the network - node cannot start exponentially backing off its lambda
    // exactly when it is kMaxSteps behind the network as it would reach kMaxLambda lambda time before catching up. If
    // we delay triggering exponential backoff by 4 steps, node should get within 1 round with the network.
    // !!! Important: This is true only for values kMinLambda = 15000ms and kMaxLambda = 60000 ms
    if (network_next_voting_step > step_ && network_next_voting_step - step_ >= kMaxSteps - 4 /* hardcoded delay */) {
      // Reset it only if it was already increased compared to default value
      if (lambda_ != kMinLambda) {
        lambda_ = kMinLambda;
        LOG(log_nf_) << "Node is " << network_next_voting_step - step_
                     << " steps behind the rest of the network. Reset lambda to the default value " << lambda_.count()
                     << " [ms]";
      }
    } else if (lambda_ < kMaxLambda) {
      // Node is < kMaxSteps steps behind the rest (at least 1/3) of the network - start exponentially backing off
      // lambda until it reaches kMaxLambdagetNetworkTplusOneNextVotingStep
      // Note: We calculate the lambda for a step independently of prior steps in case missed earlier steps.
      lambda_ *= 2;
      if (lambda_ > kMaxLambda) {
        lambda_ = kMaxLambda;
      }

      LOG(log_nf_) << "No round progress - exponentially backing off lambda to " << lambda_.count() << " [ms] in step "
                   << step_;
    }
  }
}

void PbftManager::resetStep() {
  step_ = 1;
  lambda_ = kMinLambda;
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

  auto pbft_block = getValidPbftProposedBlock(current_pbft_period, certified_block_hash);
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

  const auto new_period = getPbftPeriod();

  // Cleanup previous period votes in vote manager
  // !!!Important: we need previous period votes to get reward votes for current period block
  vote_mgr_->cleanupVotesByPeriod(new_period - 1);

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

  LOG(log_dg_) << "Reset PBFT consensus to: period " << getPbftPeriod() << ", round " << round << ", step 1";

  // Reset broadcast counters
  broadcast_votes_counter_ = 1;
  rebroadcast_votes_counter_ = 1;

  // Update current round and reset step to 1
  round_ = round;
  resetStep();
  state_ = value_proposal_state;

  // Update in DB first
  auto batch = db_->createWriteBatch();
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
  vote_mgr_->setCurrentPbftPeriodAndRound(getPbftPeriod(), round);

  current_round_start_datetime_ = std::chrono::system_clock::now();
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
  lambda_ = kMinLambda;

  const auto current_pbft_period = getPbftPeriod();
  const auto current_pbft_round = db_->getPbftMgrField(PbftMgrField::Round);
  auto current_pbft_step = db_->getPbftMgrField(PbftMgrField::Step);
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
  next_step_time_ms_ = 2 * lambda_;
}

void PbftManager::setCertifyState_() {
  state_ = certify_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 2 * lambda_;
  printCertStepInfo_ = true;
}

void PbftManager::setFinishState_() {
  LOG(log_dg_) << "Will go to first finish State";
  state_ = finish_state;
  setPbftStep(step_ + 1);
  next_step_time_ms_ = 4 * lambda_;
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
  auto gossipVotes = [this, &net](const std::vector<std::shared_ptr<Vote>> &votes, const std::string &votes_type_str,
                                  bool rebroadcast) {
    if (!votes.empty()) {
      LOG(log_dg_) << "Broadcast " << votes_type_str << " for period " << votes.back()->getPeriod() << ", round "
                   << votes.back()->getRound();
      net->gossipVotesBundle(votes, rebroadcast);
    }
  };

  // (Re)broadcast 2t+1 soft/reward/previous round next votes + all own votes
  auto broadcastVotes = [this, &net, &gossipVotes](bool rebroadcast) {
    auto [round, period] = getPbftRoundAndPeriod();

    // Broadcast 2t+1 soft votes
    gossipVotes(vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round, TwoTPlusOneVotedBlockType::SoftVotedBlock),
                "2t+1 soft votes", rebroadcast);

    // Broadcast reward votes - previous round 2t+1 cert votes
    gossipVotes(vote_mgr_->getRewardVotes(), "2t+1 propose reward votes", rebroadcast);

    // Broadcast previous round 2t+1 next votes
    if (round > 1) {
      gossipVotes(
          vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock),
          "2t+1 next votes", rebroadcast);
      gossipVotes(
          vote_mgr_->getTwoTPlusOneVotedBlockVotes(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock),
          "2t+1 next null votes", rebroadcast);
    }

    // Broadcast own votes - send votes by one as they have different type, period, round, step
    if (const auto &own_votes = vote_mgr_->getOwnVerifiedVotes(); !own_votes.empty()) {
      for (const auto &vote : own_votes) {
        net->gossipVote(vote, getPbftProposedBlock(vote->getPeriod(), vote->getBlockHash()), rebroadcast);
      }

      LOG(log_dg_) << "Broadcast own votes for period " << period << ", round " << round;
    }
  };

  const auto round_elapsed_time = elapsedTimeInMs(current_round_start_datetime_);
  const auto period_elapsed_time = elapsedTimeInMs(current_period_start_datetime_);

  if (round_elapsed_time / kMinLambda > kRebroadcastVotesLambdaTime * rebroadcast_votes_counter_) {
    // Stalled in the same round for kRebroadcastVotesLambdaTime * kMinLambda time -> rebroadcast votes
    broadcastVotes(true);
    rebroadcast_votes_counter_++;
    // If there was a rebroadcast no need to do next broadcast either
    broadcast_votes_counter_++;
  } else if (round_elapsed_time / kMinLambda > kBroadcastVotesLambdaTime * broadcast_votes_counter_) {
    // Stalled in the same round for kBroadcastVotesLambdaTime * kMinLambda time -> broadcast votes
    broadcastVotes(false);
    broadcast_votes_counter_++;
  } else if (period_elapsed_time / kMinLambda > kRebroadcastVotesLambdaTime * rebroadcast_reward_votes_counter_) {
    // Stalled in the same period for kRebroadcastVotesLambdaTime * kMinLambda time -> rebroadcast reward votes
    gossipVotes(vote_mgr_->getRewardVotes(), "2t+1 propose reward votes", true);
    rebroadcast_reward_votes_counter_++;
    // If there was a rebroadcast no need to do next broadcast either
    broadcast_reward_votes_counter_++;
  } else if (period_elapsed_time / kMinLambda > kBroadcastVotesLambdaTime * broadcast_reward_votes_counter_) {
    // Stalled in the same period for kBroadcastVotesLambdaTime * kMinLambda time -> broadcast reward votes
    gossipVotes(vote_mgr_->getRewardVotes(), "2t+1 propose reward votes", false);
    broadcast_reward_votes_counter_++;
  }
}

void PbftManager::testBroadcatVotesFunctionality() {
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
  LOG(log_tr_) << "PBFT current round: " << round << ", period: " << period << ", step " << step_;

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

  // If node is not eligible to vote and create block, always return true so pbft state machine never enters specific
  // consensus steps (propose, soft-vote, cert-vote, next-vote). Nodes that have no delegation should just
  // observe 2t+1 cert votes to move to the next period or 2t+1 next votes to move to the next round
  if (!canParticipateInConsensus(period - 1)) {
    // Check 2t+1 cert/next votes every kPollingIntervalMs
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
    return true;
  }

  return false;
}

std::shared_ptr<PbftBlock> PbftManager::getValidPbftProposedBlock(PbftPeriod period, const blk_hash_t &block_hash) {
  const auto block_data = proposed_blocks_.getPbftProposedBlock(period, block_hash);
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

    proposed_blocks_.markBlockAsValid(block);
  }

  return block;
}

bool PbftManager::placeVote(const std::shared_ptr<Vote> &vote, std::string_view log_vote_id,
                            const std::shared_ptr<PbftBlock> &voted_block) {
  if (!vote_mgr_->addVerifiedVote(vote)) {
    LOG(log_er_) << "Unable to place vote " << vote->getHash() << " for block " << vote->getBlockHash() << ", period "
                 << vote->getPeriod() << ", round " << vote->getRound() << ", step " << vote->getStep();
    return false;
  }

  gossipNewVote(vote, voted_block);

  // Save own verified vote
  vote_mgr_->saveOwnVerifiedVote(vote);

  LOG(log_nf_) << "Placed " << log_vote_id << " " << vote->getHash() << " for block " << vote->getBlockHash()
               << ", vote weight " << *vote->getWeight() << ", period " << vote->getPeriod() << ", round "
               << vote->getRound() << ", step " << vote->getStep();

  return true;
}

bool PbftManager::genAndPlaceProposeVote(const std::shared_ptr<PbftBlock> &proposed_block,
                                         std::vector<std::shared_ptr<Vote>> &&reward_votes) {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();
  const auto current_pbft_step = getPbftStep();

  if (proposed_block->getPeriod() != current_pbft_period) {
    LOG(log_er_) << "Propose block " << proposed_block->getBlockHash()
                 << " has different period than current pbft period " << current_pbft_period;
    return false;
  }

  auto propose_vote = vote_mgr_->generateVoteWithWeight(proposed_block->getBlockHash(), PbftVoteTypes::propose_vote,
                                                        current_pbft_period, current_pbft_round, current_pbft_step);
  if (!propose_vote) {
    LOG(log_nf_) << "Unable to generate propose vote";
    return false;
  }

  // Broadcast reward votes - previous round 2t+1 cert votes
  if (auto net = network_.lock()) {
    LOG(log_dg_) << "Broadcast propose block reward votes for block " << proposed_block->getBlockHash()
                 << ", num of reward votes: " << reward_votes.size() << ", period " << current_pbft_period << ", round "
                 << current_pbft_round;
    net->gossipVotesBundle(reward_votes, false);
  }

  if (!placeVote(propose_vote, "propose vote", proposed_block)) {
    LOG(log_er_) << "Unable place propose vote";
    return false;
  }

  proposed_blocks_.pushProposedPbftBlock(proposed_block, propose_vote);

  return true;
}

void PbftManager::gossipNewVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &voted_block) {
  assert(!voted_block || vote->getBlockHash() == voted_block->getBlockHash());

  auto net = network_.lock();
  if (!net) {
    LOG(log_er_) << "Could not obtain net - cannot gossip new vote";
    assert(false);
    return;
  }

  net->gossipVote(vote, voted_block);

  auto found_voted_block_it = current_round_broadcasted_votes_.find(vote->getBlockHash());
  if (found_voted_block_it == current_round_broadcasted_votes_.end()) {
    found_voted_block_it = current_round_broadcasted_votes_.insert({vote->getBlockHash(), {}}).first;
  }

  found_voted_block_it->second.emplace_back(vote->getStep());
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
    if (auto proposed_block = proposePbftBlock(); proposed_block.has_value()) {
      genAndPlaceProposeVote(proposed_block->first, std::move(proposed_block->second));
    }

    return;
  } else if (const auto previous_round_next_voted_value =
                 vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
             previous_round_next_voted_value.has_value()) {
    // previous_round_next_voted_value_ should never have value for round == 1
    assert(round > 1);

    // Round greater than 1 and next voted some value that is not null block hash
    const auto &next_voted_block_hash = *previous_round_next_voted_value;

    const auto next_voted_block = getValidPbftProposedBlock(period, next_voted_block_hash);
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
    const auto leader_block = identifyLeaderBlock_(round, period);
    if (!leader_block) {
      LOG(log_dg_) << "No leader block identified. Period " << period << ", round " << round;
      return;
    }

    assert(leader_block->getPeriod() == period);
    LOG(log_dg_) << "Leader block identified " << leader_block->getBlockHash() << ", period " << period << ", round "
                 << round;

    if (auto vote = vote_mgr_->generateVoteWithWeight(leader_block->getBlockHash(), PbftVoteTypes::soft_vote,
                                                      leader_block->getPeriod(), round, step_);
        vote) {
      placeVote(vote, "soft vote", leader_block);
    }
  } else if (const auto previous_round_next_voted_value =
                 vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
             previous_round_next_voted_value.has_value()) {
    const auto &next_voted_block_hash = *previous_round_next_voted_value;
    const auto next_voted_block = getValidPbftProposedBlock(period, next_voted_block_hash);
    if (!next_voted_block) {
      // This should never happen - if so, we probably have a bug in storing the blocks in proposed_blocks_
      LOG(log_er_) << "Unable to soft-vote previous round next voted block " << next_voted_block_hash << ", period "
                   << period << ", round " << round;
      return;
    }

    if (auto vote =
            vote_mgr_->generateVoteWithWeight(next_voted_block_hash, PbftVoteTypes::soft_vote, period, round, step_);
        vote) {
      placeVote(vote, "previous round next voted block soft vote", next_voted_block);
    }
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
  go_finish_state_ = elapsed_time_in_round > 4 * lambda_ - kPollingIntervalMs;
  if (go_finish_state_) {
    LOG(log_dg_) << "Step 3 expired, will go to step 4 in period " << period << ", round " << round;
    return;
  }

  // Should not happen, add log here for safety checking
  if (elapsed_time_in_round < 2 * lambda_) {
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
  const auto soft_voted_block = getValidPbftProposedBlock(period, *soft_voted_block_hash);
  if (soft_voted_block == nullptr) {
    LOG(log_dg_) << "Certify: invalid 2t+1 soft voted block " << *soft_voted_block_hash << ". Period " << period
                 << ",  round " << round;
    return;
  }

  // generate cert vote
  auto vote = vote_mgr_->generateVoteWithWeight(soft_voted_block->getBlockHash(), PbftVoteTypes::cert_vote,
                                                soft_voted_block->getPeriod(), round, step_);
  if (!vote) {
    LOG(log_dg_) << "Failed to generate cert vote for " << soft_voted_block->getBlockHash();
    return;
  }

  if (!placeVote(vote, "cert vote", soft_voted_block)) {
    LOG(log_er_) << "Failed to place cert vote for " << soft_voted_block->getBlockHash();
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

    const auto vote = vote_mgr_->generateVoteWithWeight(cert_voted_block->getBlockHash(), PbftVoteTypes::next_vote,
                                                        cert_voted_block->getPeriod(), round, step_);
    if (vote) {
      placeVote(vote, "first finish next vote", cert_voted_block);
    }
  } else if (round >= 2 &&
             vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedNullBlock)
                 .has_value()) {
    // Starting value in round 1 is always null block hash... So combined with other condition for next
    // voting null block hash...
    if (auto vote = vote_mgr_->generateVoteWithWeight(kNullBlockHash, PbftVoteTypes::next_vote, period, round, step_);
        vote) {
      placeVote(vote, "first finish next vote", nullptr);
    }
  } else {
    // TODO: We should vote for any value that we first saw 2t+1 next votes for in previous round -> in current design
    // we dont know for which value we saw 2t+1 next votes as first so we prefer specific block if possible
    std::pair<blk_hash_t, std::shared_ptr<PbftBlock>> starting_value;

    const auto previous_round_next_voted_value =
        vote_mgr_->getTwoTPlusOneVotedBlock(period, round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
    if (previous_round_next_voted_value.has_value()) {
      auto block = getValidPbftProposedBlock(period, *previous_round_next_voted_value);
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

    const auto vote =
        vote_mgr_->generateVoteWithWeight(starting_value.first, PbftVoteTypes::next_vote, period, round, step_);
    if (vote) {
      placeVote(vote, "starting value first finish next vote", starting_value.second);
    }
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
    const auto soft_voted_block = getValidPbftProposedBlock(period, *soft_voted_block_hash);
    if (soft_voted_block == nullptr) {
      LOG(log_dg_) << "Second finish: invalid 2t+1 soft voted block " << *soft_voted_block_hash << ". Period " << period
                   << ",  round " << round;
      return;
    }

    const auto vote =
        vote_mgr_->generateVoteWithWeight(*soft_voted_block_hash, PbftVoteTypes::next_vote, period, round, step_);
    if (vote && placeVote(vote, "second finish vote", soft_voted_block)) {
      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedSoftValue, true);
      already_next_voted_value_ = true;
    }
  };

  // Try to next vote 2t+1 soft voted block from current round
  next_vote_soft_voted_block();

  // Lambda function for next voting 2t+1 next voted null block from previous round
  auto next_vote_null_block = [this, period = period, round = round]() {
    if (already_next_voted_null_block_hash_ || round < 2) {
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

    const auto vote = vote_mgr_->generateVoteWithWeight(kNullBlockHash, PbftVoteTypes::next_vote, period, round, step_);
    if (vote && placeVote(vote, "second finish next vote", nullptr)) {
      db_->savePbftMgrStatus(PbftMgrStatus::NextVotedNullBlockHash, true);
      already_next_voted_null_block_hash_ = true;
    }
  };

  // Try to next vote 2t+1 next voted null block from previous round
  next_vote_null_block();

  loop_back_finish_state_ = elapsedTimeInMs(second_finish_step_start_datetime_) > 2 * (lambda_ - kPollingIntervalMs);
}

std::optional<std::pair<std::shared_ptr<PbftBlock>, std::vector<std::shared_ptr<Vote>>>> PbftManager::generatePbftBlock(
    PbftPeriod propose_period, const blk_hash_t &prev_blk_hash, const blk_hash_t &anchor_hash,
    const blk_hash_t &order_hash) {
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

  h256 last_state_root;
  if (propose_period > final_chain_->delegation_delay()) {
    if (const auto header = final_chain_->block_header(propose_period - final_chain_->delegation_delay())) {
      last_state_root = header->state_root;
    } else {
      LOG(log_wr_) << "Block for period " << propose_period << " could not be proposed as we are behind";
      return {};
    }
  }
  try {
    auto block = std::make_shared<PbftBlock>(prev_blk_hash, anchor_hash, order_hash, last_state_root, propose_period,
                                             node_addr_, node_sk_, std::move(reward_votes_hashes));

    return {std::make_pair(std::move(block), std::move(reward_votes))};
  } catch (const std::exception &e) {
    LOG(log_er_) << "Block for period " << propose_period << " could not be proposed " << e.what();
    return {};
  }
}

void PbftManager::processProposedBlock(const std::shared_ptr<PbftBlock> &proposed_block,
                                       const std::shared_ptr<Vote> &propose_vote) {
  if (proposed_blocks_.isInProposedBlocks(propose_vote->getPeriod(), propose_vote->getBlockHash())) {
    return;
  }

  proposed_blocks_.pushProposedPbftBlock(proposed_block, propose_vote);
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

blk_hash_t PbftManager::calculateOrderHash(const std::vector<DagBlock> &dag_blocks) {
  if (dag_blocks.empty()) {
    return kNullBlockHash;
  }
  dev::RLPStream order_stream(1);
  order_stream.appendList(dag_blocks.size());
  for (auto const &blk : dag_blocks) {
    order_stream << blk.getHash();
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

std::optional<std::pair<std::shared_ptr<PbftBlock>, std::vector<std::shared_ptr<Vote>>>>
PbftManager::proposePbftBlock() {
  const auto [current_pbft_round, current_pbft_period] = getPbftRoundAndPeriod();
  if (!vote_mgr_->genAndValidateVrfSortition(current_pbft_period, current_pbft_round)) {
    LOG(log_dg_) << "Unable to propose block for period " << current_pbft_period << ", round " << current_pbft_round
                 << ". Invalid vrf sortition";
    return {};
  }

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  auto last_period_dag_anchor_block_hash = pbft_chain_->getLastNonNullPbftBlockAnchor();
  if (last_period_dag_anchor_block_hash == kNullBlockHash) {
    last_period_dag_anchor_block_hash = dag_genesis_block_hash_;
  }

  auto ghost = dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at least include the last period dag anchor block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
  }

  blk_hash_t dag_block_hash;
  if (ghost.size() <= config_.dag_blocks_size) {
    // Move back config_.ghost_path_move_back DAG blocks for DAG sycning
    auto ghost_index =
        (ghost.size() < config_.ghost_path_move_back + 1) ? 0 : (ghost.size() - 1 - config_.ghost_path_move_back);
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = ghost[ghost_index];
  } else {
    dag_block_hash = ghost[config_.dag_blocks_size - 1];
  }

  if (dag_block_hash == dag_genesis_block_hash_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
  }

  // Compare with last dag block hash. If they are same, which means no new dag blocks generated since last round. In
  // that case PBFT proposer should propose NULL BLOCK HASH anchor as their value
  if (dag_block_hash == last_period_dag_anchor_block_hash) {
    LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                 << " No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    LOG(log_dg_) << "Ghost: " << ghost;
    return generatePbftBlock(current_pbft_period, last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
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

    if (total_weight + dag_block_weight > config_.gas_limit) {
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
  if (auto proposed_block = generatePbftBlock(current_pbft_period, last_pbft_block_hash, dag_block_hash, order_hash);
      proposed_block.has_value()) {
    LOG(log_nf_) << "Created PBFT block: " << proposed_block->first->getBlockHash() << ", order hash:" << order_hash
                 << ", DAG order " << dag_block_order;
    return proposed_block;
  }

  return {};
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

// TODO: exchange round <-> period
std::shared_ptr<PbftBlock> PbftManager::identifyLeaderBlock_(PbftRound round, PbftPeriod period) {
  LOG(log_tr_) << "Identify leader block, in period " << period << ", round " << round;

  // Get all proposal votes in the round
  auto votes = vote_mgr_->getProposalVotes(period, round);

  // Each leader candidate with <vote_signature_hash, vote>
  std::map<h256, std::shared_ptr<Vote>> leader_candidates;

  for (const auto &v : votes) {
    const auto proposed_block_hash = v->getBlockHash();
    if (proposed_block_hash == kNullBlockHash) {
      LOG(log_er_) << "Propose block hash should not be NULL. Vote " << v;
      continue;
    }

    leader_candidates[getProposal(v)] = v;
  }

  std::shared_ptr<PbftBlock> empty_leader_block;
  for (auto const &leader_vote : leader_candidates) {
    const auto proposed_block_hash = leader_vote.second->getBlockHash();

    if (pbft_chain_->findPbftBlockInChain(proposed_block_hash)) {
      continue;
    }

    auto leader_block = getValidPbftProposedBlock(leader_vote.second->getPeriod(), proposed_block_hash);
    if (!leader_block) {
      LOG(log_er_) << "Unable to get valid proposed block " << proposed_block_hash;
      continue;
    }

    if (leader_block->getPivotDagBlockHash() == kNullBlockHash && empty_leader_block == nullptr) {
      empty_leader_block = leader_block;
      continue;
    }
    return leader_block;
  }

  // no eligible leader
  return empty_leader_block;
}

PbftStateRootValidation PbftManager::validatePbftBlockStateRoot(const std::shared_ptr<PbftBlock> &pbft_block) const {
  auto period = pbft_block->getPeriod();
  auto const &pbft_block_hash = pbft_block->getBlockHash();
  {
    h256 prev_state_root_hash;
    if (period > final_chain_->delegation_delay()) {
      if (const auto header = final_chain_->block_header(period - final_chain_->delegation_delay())) {
        prev_state_root_hash = header->state_root;
      } else {
        LOG(log_wr_) << "Block " << pbft_block_hash << " could not be validated as we are behind";
        return PbftStateRootValidation::Missing;
      }
    }
    if (pbft_block->getPrevStateRoot() != prev_state_root_hash) {
      LOG(log_er_) << "Block " << pbft_block_hash << " state root " << pbft_block->getPrevStateRoot()
                   << " isn't matching actual " << prev_state_root_hash;
      return PbftStateRootValidation::Invalid;
    }
  }
  return PbftStateRootValidation::Valid;
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

  if (validatePbftBlockStateRoot(pbft_block) != PbftStateRootValidation::Valid) {
    return false;
  }

  // Validates reward votes
  if (!vote_mgr_->checkRewardVotes(pbft_block, false).first) {
    LOG(log_er_) << "Failed verifying reward votes for proposed PBFT block " << pbft_block_hash;
    return false;
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
    anchor_dag_block_order_cache_[anchor_hash].emplace_back(std::move(*dag_block));
  }

  auto last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    auto ghost = dag_mgr_->getGhostPath(prev_pbft_block.getPivotDagBlockHash());
    if (ghost.size() > 1 && anchor_hash != ghost[1]) {
      if (!checkBlockWeight(anchor_dag_block_order_cache_[anchor_hash])) {
        LOG(log_er_) << "PBFT block " << pbft_block_hash << " weight exceeded max limit";
        anchor_dag_block_order_cache_.erase(anchor_hash);
        return false;
      }
    }
  }

  return true;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(const std::shared_ptr<PbftBlock> &pbft_block,
                                                   std::vector<std::shared_ptr<Vote>> &&current_round_cert_votes) {
  PeriodData period_data;
  period_data.pbft_blk = pbft_block;
  if (pbft_block->getPivotDagBlockHash() != kNullBlockHash) {
    auto dag_order_it = anchor_dag_block_order_cache_.find(pbft_block->getPivotDagBlockHash());
    assert(dag_order_it != anchor_dag_block_order_cache_.end());
    std::unordered_set<trx_hash_t> trx_set;
    std::vector<trx_hash_t> transactions_to_query;
    period_data.dag_blocks.reserve(dag_order_it->second.size());
    for (const auto &dag_blk : dag_order_it->second) {
      for (const auto &trx_hash : dag_blk.getTrxs()) {
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
                            bool synchronous_processing) {
  std::shared_ptr<DagBlock> anchor_block = nullptr;

  if (const auto anchor = period_data.pbft_blk->getPivotDagBlockHash()) {
    anchor_block = dag_mgr_->getDagBlock(anchor);
    if (!anchor_block) {
      LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor << " in DB.";
      assert(false);
    }
  }

  const auto result =
      final_chain_->finalize(std::move(period_data), std::move(finalized_dag_blk_hashes), std::move(anchor_block));
  if (synchronous_processing) {
    result.wait();
  }
}

bool PbftManager::pushPbftBlock_(PeriodData &&period_data, std::vector<std::shared_ptr<Vote>> &&cert_votes) {
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

  assert(cert_votes.empty() == false);
  assert(pbft_block_hash == cert_votes[0]->getBlockHash());

  auto pbft_period = period_data.pbft_blk->getPeriod();
  auto null_anchor = period_data.pbft_blk->getPivotDagBlockHash() == kNullBlockHash;

  auto batch = db_->createWriteBatch();

  LOG(log_dg_) << "Storing pbft blk " << pbft_block_hash << " cert votes: " << cert_votes;
  // Update PBFT chain head block
  db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStrForBlock(pbft_block_hash, null_anchor),
                          batch);

  vec_blk_t dag_blocks_order;
  dag_blocks_order.reserve(period_data.dag_blocks.size());
  std::transform(period_data.dag_blocks.begin(), period_data.dag_blocks.end(), std::back_inserter(dag_blocks_order),
                 [](const DagBlock &dag_block) { return dag_block.getHash(); });

  // We need to reorder transactions before saving them
  reorderTransactions(period_data.transactions);

  db_->savePeriodData(period_data, batch);

  // Replace current reward votes
  vote_mgr_->resetRewardVotes(cert_votes[0]->getPeriod(), cert_votes[0]->getRound(), cert_votes[0]->getStep(),
                              cert_votes[0]->getBlockHash(), batch);

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

  finalize_(std::move(period_data), std::move(dag_blocks_order));

  db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, true);
  executed_pbft_block_ = true;

  // Advance pbft consensus period
  advancePeriod();

  return true;
}

PbftPeriod PbftManager::pbftSyncingPeriod() const {
  return std::max(sync_queue_.getPeriod(), pbft_chain_->getPbftChainSize());
}

std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>> PbftManager::processPeriodData() {
  auto [period_data, cert_votes, node_id] = sync_queue_.pop();
  auto pbft_block_hash = period_data.pbft_blk->getBlockHash();
  LOG(log_dg_) << "Pop pbft block " << pbft_block_hash << " from synced queue";

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
    auto validation_result = validatePbftBlockStateRoot(period_data.pbft_blk);
    if (validation_result != PbftStateRootValidation::Missing) {
      if (validation_result == PbftStateRootValidation::Invalid) {
        LOG(log_er_) << "Failed verifying block " << pbft_block_hash
                     << " with invalid state root: " << period_data.pbft_blk->getPrevStateRoot()
                     << ". Disconnect malicious peer " << node_id.abridged();
        sync_queue_.clear();
        net->handleMaliciousSyncPeer(node_id);
        return std::nullopt;
      }
      break;
    }
    // If syncing and pbft manager is faster than execution a delay might be needed to allow EVM to catch up
    final_chain_->wait_for_finalized();
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

  // Get all the ordered unique non-finalized transactions which should match period_data.transactions
  std::unordered_set<trx_hash_t> trx_set;
  std::vector<trx_hash_t> transactions_to_query;
  for (auto const &dag_block : period_data.dag_blocks) {
    for (auto const &trx_hash : dag_block.getTrxs()) {
      if (trx_set.insert(trx_hash).second) {
        transactions_to_query.emplace_back(trx_hash);
      }
    }
  }
  auto non_finalized_transactions = trx_mgr_->excludeFinalizedTransactions(transactions_to_query);

  if (non_finalized_transactions.size() != period_data.transactions.size()) {
    LOG(log_er_) << "Synced PBFT block " << pbft_block_hash << " transactions count " << period_data.transactions.size()
                 << " incorrect, expected: " << non_finalized_transactions.size();
    sync_queue_.clear();
    net->handleMaliciousSyncPeer(node_id);
    return std::nullopt;
  }
  for (uint32_t i = 0; i < period_data.transactions.size(); i++) {
    if (!non_finalized_transactions.contains(period_data.transactions[i]->getHash())) {
      LOG(log_er_) << "Synced PBFT block " << pbft_block_hash << " has incorrect transaction "
                   << period_data.transactions[i]->getHash();
      sync_queue_.clear();
      net->handleMaliciousSyncPeer(node_id);
      return std::nullopt;
    }
  }

  return std::optional<std::pair<PeriodData, std::vector<std::shared_ptr<Vote>>>>(
      {std::move(period_data), std::move(cert_votes)});
}

bool PbftManager::validatePbftBlockCertVotes(const std::shared_ptr<PbftBlock> pbft_block,
                                             const std::vector<std::shared_ptr<Vote>> &cert_votes) const {
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

bool PbftManager::canParticipateInConsensus(PbftPeriod period) const {
  try {
    return final_chain_->dpos_is_eligible(period, node_addr_);
  } catch (state_api::ErrFutureBlock &e) {
    LOG(log_er_) << "Unable to decide if node is consensus node or not for period: " << period
                 << ". Period is too far ahead of actual finalized pbft chain size ("
                 << final_chain_->last_block_number() << "). Err msg: " << e.what()
                 << ". Node is considered as not eligible to participate in consensus for period " << period;
  }

  return false;
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
                                      std::vector<std::shared_ptr<Vote>> &&current_block_cert_votes) {
  const auto period = period_data.pbft_blk->getPeriod();
  if (!sync_queue_.push(std::move(period_data), node_id, pbft_chain_->getPbftChainSize(),
                        std::move(current_block_cert_votes))) {
    LOG(log_er_) << "Trying to push period data with " << period << " period, but current period is "
                 << sync_queue_.getPeriod();
  }
}

size_t PbftManager::periodDataQueueSize() const { return sync_queue_.size(); }

bool PbftManager::checkBlockWeight(const std::vector<DagBlock> &dag_blocks) const {
  const u256 total_weight =
      std::accumulate(dag_blocks.begin(), dag_blocks.end(), u256(0),
                      [](u256 value, const auto &dag_block) { return value + dag_block.getGasEstimation(); });
  if (total_weight > config_.gas_limit) {
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

}  // namespace taraxa
