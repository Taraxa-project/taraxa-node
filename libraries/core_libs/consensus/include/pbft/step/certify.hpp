#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

/**
 * @ingroup PBFT
 * @brief Certify step class
 */
class Certify : public Step {
 public:
  Certify(std::shared_ptr<RoundFace> round) : Step(StepType::certify, 4 * round->getLambda(), std::move(round)) {}
  void run() override;
};

}  // namespace taraxa::step
