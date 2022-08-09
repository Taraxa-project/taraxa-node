#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

/**
 * @ingroup PBFT
 * @brief Polling step class
 */
class Polling : public Step {
 public:
  Polling(uint64_t id, std::shared_ptr<RoundFace> round) : Step(id, (id + 1) * round->getLambda(), std::move(round)) {
    assert(id % 2 == 1);
    LOG(log_dg_) << "Will go to second finish State";
  }
  void run() override;

 private:
  void finish() override;
};

}  // namespace taraxa::step