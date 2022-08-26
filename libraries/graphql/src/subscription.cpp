#include "subscription.hpp"

#include <iostream>

namespace graphql::taraxa {

service::FieldResult<response::Value> Subscription::getTestSubscription(service::FieldParams&& params) const {
  std::cout << "Subscription::getTestSubscription" << std::endl;
  return response::Value(123456789);
}

}  // namespace graphql::taraxa