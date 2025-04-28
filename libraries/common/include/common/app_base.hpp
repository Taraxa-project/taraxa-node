#pragma once

#include <libdevcore/Address.h>
#include <libdevcrypto/Common.h>

#include <memory>

#include "config/config.hpp"

namespace taraxa {
struct FullNodeConfig;
class Network;
class TransactionManager;
class DagManager;
class DbStorage;
class PbftManager;
class VoteManager;
class PbftChain;
class DagBlockProposer;
class GasPricer;

namespace final_chain {
class FinalChain;
}
namespace pillar_chain {
class PillarChainManager;
}

namespace metrics {
class MetricsService;
}

class AppBase {
 public:
  AppBase() {}

  virtual ~AppBase() = default;

  virtual const FullNodeConfig &getConfig() const = 0;
  virtual std::shared_ptr<Network> getNetwork() const = 0;
  virtual std::shared_ptr<TransactionManager> getTransactionManager() const = 0;
  virtual std::shared_ptr<DagManager> getDagManager() const = 0;
  virtual std::shared_ptr<DbStorage> getDB() const = 0;
  virtual std::shared_ptr<PbftManager> getPbftManager() const = 0;
  virtual std::shared_ptr<VoteManager> getVoteManager() const = 0;
  virtual std::shared_ptr<PbftChain> getPbftChain() const = 0;
  virtual std::shared_ptr<final_chain::FinalChain> getFinalChain() const = 0;
  virtual std::shared_ptr<metrics::MetricsService> getMetrics() const = 0;
  // used only in tests
  virtual std::shared_ptr<DagBlockProposer> getDagBlockProposer() const = 0;
  virtual std::shared_ptr<GasPricer> getGasPricer() const = 0;

  const dev::Address &getAddress() const { return conf_.getFirstWallet().node_addr; }
  const Secret &getSecretKey() const { return conf_.getFirstWallet().node_secret; }
  vrf_wrapper::vrf_sk_t getVrfSecretKey() const { return conf_.getFirstWallet().vrf_secret; }

  virtual std::shared_ptr<pillar_chain::PillarChainManager> getPillarChainManager() const = 0;

  bool isStarted() const { return started_; }

  virtual void start() = 0;

 protected:
  // configuration
  FullNodeConfig conf_;

  std::atomic_bool started_ = 0;
  std::atomic_bool stopped_ = true;
};

}  // namespace taraxa
