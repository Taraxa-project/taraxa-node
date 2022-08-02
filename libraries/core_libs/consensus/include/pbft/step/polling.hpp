#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Polling : public Step {
 public:
  Polling(uint64_t id, std::shared_ptr<RoundFace> round) : Step(id, std::move(round)) {
    init();
    assert(id % 2 == 1);
  }
  void run() override;
  void finish_() override;

 private:
  void init() override;
};

}  // namespace taraxa::step