#pragma once

#include <json/json.h>

#include <functional>
#include <list>
#include <map>

enum class SubscriptionType {
  HEADS,
  DAG_BLOCKS,
  TRANSACTIONS,
  DAG_BLOCK_FINALIZED,
  PBFT_BLOCK_EXECUTED,
  PILLAR_BLOCK,
};

namespace taraxa::net {

class Subscription {
 public:
  Subscription(int id) : id_(id) {}
  virtual ~Subscription() = default;
  virtual SubscriptionType getType() const = 0;
  int getId() const { return id_; }
  virtual std::string processPayload(Json::Value payload) const = 0;

 protected:
  int id_;
};

class HeadsSubscription : public Subscription {
 public:
  explicit HeadsSubscription(int id, bool hash_only = false) : Subscription(id), hash_only_(hash_only) {}
  static constexpr SubscriptionType type = SubscriptionType::HEADS;

  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;

 private:
  bool hash_only_ = false;
};

class DagBlocksSubscription : public Subscription {
 public:
  explicit DagBlocksSubscription(int id, bool hash_only = false) : Subscription(id), hash_only_(hash_only) {}
  static constexpr SubscriptionType type = SubscriptionType::DAG_BLOCKS;
  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;

 private:
  bool hash_only_ = false;
};

class TransactionsSubscription : public Subscription {
 public:
  explicit TransactionsSubscription(int id) : Subscription(id) {}
  static constexpr SubscriptionType type = SubscriptionType::TRANSACTIONS;
  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;
};

class DagBlockFinalizedSubscription : public Subscription {
 public:
  explicit DagBlockFinalizedSubscription(int id) : Subscription(id) {}
  static constexpr SubscriptionType type = SubscriptionType::DAG_BLOCK_FINALIZED;
  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;
};

class PbftBlockExecutedSubscription : public Subscription {
 public:
  explicit PbftBlockExecutedSubscription(int id) : Subscription(id) {}
  static constexpr SubscriptionType type = SubscriptionType::PBFT_BLOCK_EXECUTED;
  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;
};

class PillarBlockSubscription : public Subscription {
 public:
  explicit PillarBlockSubscription(int id, bool include_signatures = false)
      : Subscription(id), include_signatures_(include_signatures) {}
  static constexpr SubscriptionType type = SubscriptionType::PILLAR_BLOCK;
  SubscriptionType getType() const override { return type; }
  std::string processPayload(Json::Value payload) const override;

 private:
  bool include_signatures_ = false;
};

class Subscriptions {
 public:
  Subscriptions(std::function<void(std::string&&)> send) : send_(send) {}
  int addSubscription(std::shared_ptr<Subscription> subscription);
  bool removeSubscription(int id);
  void process(SubscriptionType type, const Json::Value& payload);

 private:
  std::function<void(std::string&&)> send_;
  std::map<int, std::shared_ptr<Subscription>> subscriptions_;
  std::map<SubscriptionType, std::list<int>> subscriptions_by_method_;
};
}  // namespace taraxa::net