// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef SRCNETGRAPHQLMUTATION_H
#define SRCNETGRAPHQLMUTATION_H

#include <libweb3jsonrpc/Eth.h>

#include <memory>
#include <string>
#include <vector>

#include "chain/final_chain.hpp"
#include "chain/state_api.hpp"
#include "gen/TaraxaSchema.h"

namespace graphql {
namespace taraxa {

class Mutation : public object::Mutation {
 public:
  explicit Mutation(std::shared_ptr<dev::rpc::Eth::NodeAPI> node_api);

  virtual service::FieldResult<response::Value> applySendRawTransaction(service::FieldParams&& params,
                                                                        response::Value&& dataArg) const override;

 private:
  std::shared_ptr<dev::rpc::Eth::NodeAPI> node_api_;
};

} /* namespace taraxa */
} /* namespace graphql */

#endif  // SRCNETGRAPHQLMUTATION_H
