#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

/**
 * @ingroup PBFT
 * @brief Filter step class
 */
class Filter : public Step {
 public:
  Filter(std::shared_ptr<RoundFace> round) : Step(StepType::filter, 2 * round->getLambda(), std::move(round)) {}
  void run() override;

 private:
  blk_hash_t identifyLeaderBlock();

  /**
   * @brief Update last soft voting value
   * @param new_soft_voted_value soft voting value
   */
  void updateLastSoftVotedValue(blk_hash_t const new_soft_voted_value);

  /**
   * @brief Calculate the lowest hash of a vote by vote weight
   * @param vote vote
   * @return lowest hash of a vote
   */
  h256 getProposal(const std::shared_ptr<Vote> &vote) const;
};

}  // namespace taraxa::step