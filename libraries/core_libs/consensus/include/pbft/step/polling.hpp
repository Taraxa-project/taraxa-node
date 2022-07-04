#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Polling : public Step {
 public:
  Polling(uint64_t id, std::shared_ptr<CommonState> state, std::shared_ptr<NodeFace> node)
      : Step(id, std::move(state), std::move(node)) {
    init();
    assert(id % 2 == 1);
  }
  void run() override;
  void finish_() override;

 private:
  void init() override;
  bool alreadyBroadcasted_ = false;
};

}  // namespace taraxa::step