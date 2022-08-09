#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

/**
 * @ingroup PBFT
 * @brief Finish step class
 */
class Finish : public Step {
 public:
  Finish(uint64_t id, std::shared_ptr<RoundFace> round) : Step(id, id * round->getLambda(), std::move(round)) {
    assert(id % 2 == 0);
    LOG(log_dg_) << "Will go to first finish State";
  }
  void run() override;
};

}  // namespace taraxa::step