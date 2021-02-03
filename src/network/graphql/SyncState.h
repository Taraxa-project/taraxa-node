#pragma once

#include <libweb3jsonrpc/Eth.h>

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql::taraxa {

class SyncState : public object::SyncState {
 public:
  explicit SyncState(std::shared_ptr<dev::rpc::Eth::NodeAPI> node_api);

  virtual service::FieldResult<response::Value> getStartingBlock(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getCurrentBlock(service::FieldParams&& params) const override;
  virtual service::FieldResult<response::Value> getHighestBlock(service::FieldParams&& params) const override;

 private:
  std::shared_ptr<dev::rpc::Eth::NodeAPI> node_api_;
};

}  // namespace graphql::taraxa
