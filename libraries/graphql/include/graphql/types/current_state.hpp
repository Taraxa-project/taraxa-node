#pragma once

#include <memory>

#include "CurrentStateObject.h"
#include "dag/dag.hpp"
#include "final_chain/final_chain.hpp"

namespace graphql::taraxa {

class CurrentState {
 public:
  explicit CurrentState(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                        std::shared_ptr<::taraxa::DagManager> dag_manager) noexcept;

  response::Value getFinalBlock() const noexcept;
  response::Value getDagBlockLevel() const noexcept;
  response::Value getDagBlockPeriod() const noexcept;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::DagManager> dag_manager_;
};

}  // namespace graphql::taraxa