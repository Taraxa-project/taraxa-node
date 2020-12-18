// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef TARAXAGQLSCHEMAIMPL_H
#define TARAXAGQLSCHEMAIMPL_H

#include <memory>
#include <string>
#include <vector>

#include "../final_chain.hpp"
#include "TaraxaGqlSchema.h"

namespace graphql {
namespace taraxa {

class Query : public object::Query {
 public:
  explicit Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain);

  virtual service::FieldResult<std::shared_ptr<object::Block>> getBlock(service::FieldParams&& params,
                                                                        std::optional<response::Value>&& numberArg,
                                                                        std::optional<response::Value>&& hashArg) const;

 private:
  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
};

class Mutation : public object::Mutation {
 public:
  explicit Mutation();

 private:
};

class Block : public object::Block {
 public:
  explicit Block(std::shared_ptr<dev::eth::BlockHeader> block_header);

  virtual service::FieldResult<response::Value> getNumber(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const;

 private:
  std::shared_ptr<dev::eth::BlockHeader> block_header_;
};

} /* namespace taraxa */
} /* namespace graphql */

#endif  // TARAXAGQLSCHEMAIMPL_H
