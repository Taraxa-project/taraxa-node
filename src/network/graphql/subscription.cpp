#include "subscription.hpp"

namespace graphql::taraxa {

service::FieldResult<response::Value> Subscription::getTestSubscription(service::FieldParams&& params) const {
  return response::Value(123456789);
}

}
