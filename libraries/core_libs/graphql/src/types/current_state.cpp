#include "graphql/types/current_state.hpp"

namespace graphql::taraxa {

CurrentState::CurrentState(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                           std::shared_ptr<::taraxa::DagManager> dag_manager) noexcept
    : final_chain_(std::move(final_chain)), dag_manager_(std::move(dag_manager)) {}

response::Value CurrentState::getFinalBlock() const noexcept {
  return response::Value(static_cast<int>(final_chain_->last_block_number()));
}

response::Value CurrentState::getDagBlockLevel() const noexcept {
  return response::Value(static_cast<int>(dag_manager_->getMaxLevel()));
}

response::Value CurrentState::getDagBlockPeriod() const noexcept {
  return response::Value(static_cast<int>(dag_manager_->getLatestPeriod()));
}

}  // namespace graphql::taraxa