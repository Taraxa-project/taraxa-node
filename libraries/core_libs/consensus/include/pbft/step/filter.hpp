#pragma once
#include "logger/logger.hpp"
#include "pbft/step/step.hpp"

namespace taraxa::step {

class Filter : public Step {
 public:
  Filter(std::shared_ptr<CommonState> state, std::shared_ptr<NodeFace> node)
      : Step(PbftStates::filter, std::move(state), std::move(node)) {
    init();
  }
  void run() override;

 private:
  void init() override;
  blk_hash_t identifyLeaderBlock_();

  /**
   * @brief Update last soft voting value
   * @param new_soft_voted_value soft voting value
   */
  void updateLastSoftVotedValue_(blk_hash_t const new_soft_voted_value);
};

}  // namespace taraxa::step