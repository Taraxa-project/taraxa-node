#include "current_state.hpp"

namespace graphql::taraxa {

CurrentState::CurrentState(const std::shared_ptr<::taraxa::final_chain::FinalChain>& final_chain,
                           const std::shared_ptr<::taraxa::DagManager>& dag_mgr)
    : final_chain_(final_chain), dag_mgr_(dag_mgr) {}

service::FieldResult<response::Value> CurrentState::getFinalBlock(service::FieldParams&& params) const {
  return response::Value(static_cast<response::IntType>(final_chain_->last_block_number()));
}

service::FieldResult<response::Value> CurrentState::getDagBlockLevel(service::FieldParams&& params) const {
  return response::Value(static_cast<response::IntType>(dag_mgr_->getMaxLevel()));
}

service::FieldResult<response::Value> CurrentState::getDagBlockPeriod(service::FieldParams&& params) const {
  return response::Value(static_cast<response::IntType>(dag_mgr_->getLatestPeriod()));
}

}  // namespace graphql::taraxa