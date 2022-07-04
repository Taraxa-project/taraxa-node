#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Certify : public Step {
 public:
  Certify(std::shared_ptr<CommonState> state, std::shared_ptr<NodeFace> node)
      : Step(PbftStates::certify, std::move(state), std::move(node)) {
    init();
  }
  void run() override;

 private:
  void init() override;
  bool should_have_cert_voted_in_this_round_ = false;
};

}  // namespace taraxa::step
