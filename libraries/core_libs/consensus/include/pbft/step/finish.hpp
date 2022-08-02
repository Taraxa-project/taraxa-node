#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Finish : public Step {
 public:
  Finish(uint64_t id, std::shared_ptr<RoundFace> round) : Step(id, std::move(round)) {
    init();
    assert(id % 2 == 0);
  }
  void run() override;

 private:
  void init() override;
};

}  // namespace taraxa::step