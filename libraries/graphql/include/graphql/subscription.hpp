#pragma once

#include "SubscriptionObject.h"

namespace graphql::taraxa {

class Subscription {
 public:
  explicit Subscription() noexcept = default;

  response::Value getTestSubscription() const noexcept;
};

}  // namespace graphql::taraxa