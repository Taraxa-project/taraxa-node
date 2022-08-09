#include "pbft/round.hpp"

#include "pbft/pbft_manager.hpp"
#include "pbft/period_data.hpp"
#include "pbft/step/certify.hpp"
#include "pbft/step/filter.hpp"
#include "pbft/step/finish.hpp"
#include "pbft/step/polling.hpp"
#include "pbft/step/propose.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

Round::Round(uint64_t id, std::shared_ptr<NodeFace> node)
    : RoundFace(id, std::move(node)), LOG_OBJECTS_INITIALIZE_FROM_SHARED(node_->round_logger) {
  previous_round_next_voted_value_ = node_->next_votes_manager_->getVotedValue();
  previous_round_next_voted_null_block_hash_ = node_->next_votes_manager_->haveEnoughVotesForNullBlockHash();
}

Round::~Round() { step_.reset(); }

std::shared_ptr<Round> Round::make(uint64_t id, std::optional<uint64_t> step, std::shared_ptr<NodeFace> node) {
  auto round = std::make_shared<Round>(id, std::move(node));
  round->start(step);

  return round;
}

void Round::run() {
  while (!finished_) {
    auto time_from_round_start = std::chrono::system_clock::now() - start_time_;
    time_from_start_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(time_from_round_start);
    step_->run();
    if (!step_->finished()) {
      sleepUntil(time_from_start_ms_ + getLambda() / 4);
    } else {
      nextStep();
    }
  }
}

void Round::sleepUntil(const time_point& time) {
  std::unique_lock<std::mutex> lock(stop_mtx_);
  stop_cv_.wait_until(lock, time);
}

void Round::sleepUntil(const std::chrono::milliseconds& time) { sleepUntil(start_time_ + time); }

StepType Round::getStepType() const { return step_->getType(); }

void Round::finish() {
  finished_ = true;
  stop_cv_.notify_all();

  if (daemon_) {
    daemon_->join();
  }
  step_.reset();

  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }

  auto next_votes = node_->next_votes_manager_->getNextVotes();

  auto batch = node_->db_->createWriteBatch();

  node_->db_->addPbft2TPlus1ToBatch(kId_, pm->TWO_T_PLUS_ONE, batch);
  node_->db_->addNextVotesToBatch(kId_, next_votes, batch);
  if (kId_ > 1) {
    // Cleanup old previous round next votes
    node_->db_->removeNextVotesToBatch(kId_ - 1, batch);
  }

  node_->db_->commitWriteBatch(batch);

  // Move to a new round, cleanup previous round votes
  node_->vote_mgr_->cleanupVotes(kId_);

  if (executed_pbft_block_) {
    node_->vote_mgr_->removeVerifiedVotes();
    node_->db_->savePbftMgrStatus(PbftMgrStatus::ExecutedBlock, false);
  }
}

void Round::start(std::optional<uint64_t> step) {
  if (!step) {
    startStep<step::Propose>();
  } else {
    if (kId_ == 1 && step == 1) {
      startStep<step::Propose>();
    } else {
      if (step < 4) {
        // Node start from DB, skip step 1 or 2 or 3
        step = 4;
      }

      if (step.value() % 2 == 0) {
        // Node start from DB in first finishing state
        startStep<step::Finish>(step.value());
      } else {
        // Node start from DB in second finishing state
        startStep<step::Polling>(step.value());
      }
    }
  }

  initDbValues();

  node_->db_->savePbftMgrField(PbftMgrRoundStep::PbftRound, kId_);

  LAMBDA_ms = std::chrono::milliseconds(node_->pbft_config_.lambda_ms_min);
  LAMBDA_backoff_multiple = 1;

  // replace starting_step number with round start_time adjusting
  auto time_difference = step_->getId() * getLambda();
  if (step_->getId() % 2 == 1) {
    time_difference -= getLambda();
  }
  start_time_ = std::chrono::system_clock::now() - time_difference;

  daemon_ = std::make_unique<std::thread>([this]() { run(); });
}

void Round::initDbValues() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return;
  }
  // Update in DB first
  auto batch = node_->db_->createWriteBatch();
  // Update PBFT round and reset step to 1
  node_->db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftRound, kId_, batch);
  node_->db_->addPbftMgrFieldToBatch(PbftMgrRoundStep::PbftStep, 1, batch);

  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold,
                                            pm->getSortitionThreshold(), batch);
  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod,
                                            pm->getFinalizedDPOSPeriod(), batch);
  node_->db_->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount,
                                            pm->getDposTotalVotesCount(), batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::ExecutedInRound, false, batch);
  node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::OwnStartingValueInRound, kNullBlockHash, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedNullBlockHash, false, batch);
  node_->db_->addPbftMgrStatusToBatch(PbftMgrStatus::NextVotedSoftValue, false, batch);
  node_->db_->addPbftMgrVotedValueToBatch(PbftMgrVotedValue::SoftVotedBlockHashInRound, kNullBlockHash, batch);
  if (soft_voted_block_) {
    // Cleanup soft votes for previous round
    node_->db_->removeSoftVotesToBatch(kId_, batch);
  }
  node_->db_->commitWriteBatch(batch);
}

void Round::nextStep() {
  switch (step_->getType()) {
    case StepType::propose:
      startStep<step::Filter>();
      break;
    case StepType::filter:
      startStep<step::Certify>();
      break;
    case StepType::certify:
      startStep<step::Finish>(getStepId() + 1);
      break;
    case StepType::finish:
      startStep<step::Polling>(getStepId() + 1);
      break;
    case StepType::polling:
      startStep<step::Finish>(getStepId() + 1);
      break;
    default:
      LOG(log_er_) << "Unknown PBFT state " << step_->getType();
      assert(false);
  }
}

void Round::setStep(std::unique_ptr<Step>&& step) {
  step_ = std::move(step);
  if (step_) {
    updateStepData();
  }
}

void Round::updateStepData() {
  static std::default_random_engine random_engine{std::random_device{}()};
  auto step_id = getStepId();

  node_->db_->savePbftMgrField(PbftMgrRoundStep::PbftStep, step_id);

  if (step_id > MAX_STEPS && LAMBDA_backoff_multiple < 8) {
    // Note: We calculate the lambda for a step independently of prior steps
    //       in case missed earlier steps.
    std::uniform_int_distribution<u_long> distribution(0, step_id - MAX_STEPS);
    auto lambda_random_count = distribution(random_engine);
    LAMBDA_backoff_multiple = 2 * LAMBDA_backoff_multiple;
    LAMBDA_ms = std::chrono::milliseconds(
        std::min(kMaxLambda, node_->pbft_config_.lambda_ms_min * (LAMBDA_backoff_multiple + lambda_random_count)));
    LOG(log_dg_) << "Surpassed max steps, exponentially backing off lambda to " << LAMBDA_ms.count() << " ms in round "
                 << kId_ << ", step " << step_id;
  }
}

}  // namespace taraxa