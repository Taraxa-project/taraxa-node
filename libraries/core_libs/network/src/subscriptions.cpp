#include "network/subscriptions.hpp"

#include "common/jsoncpp.hpp"

namespace taraxa::net {
// Subscriptions::Subscriptions(int id, SubscriptionType type) : id(id), type(type) {}
int Subscriptions::addSubscription(std::shared_ptr<Subscription> subscription) {
  subscriptions_[subscription->getId()] = subscription;
  return subscription->getId();
}

bool Subscriptions::removeSubscription(int id) {
  auto sub = subscriptions_[id];
  if (!sub) {
    return false;
  }
  auto type = sub->getType();
  auto subs = subscriptions_by_method_[type];
  subs.erase(std::remove(subs.begin(), subs.end(), id));
  subscriptions_.erase(id);
  return true;
}

void Subscriptions::process(SubscriptionType type, const Json::Value& payload) {
  for (auto id : subscriptions_by_method_[type]) {
    send_(subscriptions_[id]->processPayload(payload));
  }
}

std::string makeEthSubscriptionResponse(int id, const Json::Value& payload) {
  Json::Value params;
  params["result"] = payload;
  params["subscription"] = id;
  Json::Value res;
  res["jsonrpc"] = "2.0";
  res["method"] = "eth_subscription";
  res["params"] = params;

  return util::to_string(res);
}

std::string HeadsSubscription::processPayload(Json::Value payload) const {
  if (hash_only_) {
    payload = payload["hash"];
  }
  return makeEthSubscriptionResponse(id_, payload);
}

std::string DagBlocksSubscription::processPayload(Json::Value payload) const {
  if (hash_only_) {
    payload = payload["hash"];
  }
  return makeEthSubscriptionResponse(id_, payload);
}

std::string TransactionsSubscription::processPayload(Json::Value payload) const {
  return makeEthSubscriptionResponse(id_, payload);
}

std::string DagBlockFinalizedSubscription::processPayload(Json::Value payload) const {
  return makeEthSubscriptionResponse(id_, payload);
}

std::string PbftBlockExecutedSubscription::processPayload(Json::Value payload) const {
  return makeEthSubscriptionResponse(id_, payload);
}

std::string PillarBlockSubscription::processPayload(Json::Value payload) const {
  if (!include_signatures_) {
    payload.removeMember("signatures");
  }
  return makeEthSubscriptionResponse(id_, payload);
}

}  // namespace taraxa::net