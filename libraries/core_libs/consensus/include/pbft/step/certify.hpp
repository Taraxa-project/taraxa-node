#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Certify : public Step {
 public:
  Certify(std::shared_ptr<RoundFace> round) : Step(StepType::certify, std::move(round)) { init(); }
  void run() override;

 private:
  void init() override;
  void finish_() override;
};

}  // namespace taraxa::step
