#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Finish : public Step {
 public:
  Finish(uint64_t id, std::shared_ptr<CommonState> state, std::shared_ptr<NodeFace> node)
      : Step(id, std::move(state), std::move(node)) {
    init();
    assert(id % 2 == 0);
  }
  void run() override;

 private:
  void init() override;
};

}  // namespace taraxa::step