#pragma once

#include "TaraxaSchema.h"

namespace graphql::taraxa {

class Subscription : public object::Subscription {
 public:
  explicit Subscription() = default;

  virtual service::FieldResult<response::Value> getTestSubscription(service::FieldParams&& params) const override;
};

}  // namespace graphql::taraxa