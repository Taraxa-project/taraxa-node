#pragma once

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "common/thread_pool.hpp"
#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "config/version.hpp"
#include "network/http_server.hpp"
#include "network/rpc/EthFace.h"
#include "network/rpc/NetFace.h"
#include "network/rpc/TaraxaFace.h"
#include "network/rpc/TestFace.h"
#include "network/ws_server.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction.hpp"
#include "vote_manager/vote_manager.hpp"

namespace metrics {
class MetricsService;
}
namespace taraxa {

class Network;
class DagBlockProposer;
class DagManager;
class DagBlock;
struct Transaction;
class TransactionManager;
class GasPricer;
class PbftManager;
struct NetworkConfig;
class KeyManager;

class FullNode : public std::enable_shared_from_this<FullNode> {
  using shared_ptr_t = std::shared_ptr<FullNode>;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using jsonrpc_server_t = ModularServer<net::TaraxaFace, net::NetFace, net::EthFace, net::TestFace>;

  // should be destroyed after all components, since they may depend on it through unsafe pointers
  std::unique_ptr<util::ThreadPool> rpc_thread_pool_;
  std::unique_ptr<util::ThreadPool> graphql_thread_pool_;
  // In cae we will you config for this TP, it needs to be unique_ptr !!!
  util::ThreadPool subscription_pool_;

  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  // Ethereum key pair
  dev::KeyPair kp_;
  // components
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DbStorage> old_db_;
  std::shared_ptr<GasPricer> gas_pricer_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<DagBlockProposer> dag_block_proposer_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<NextVotesManager> next_votes_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<net::HttpServer> jsonrpc_http_;
  std::shared_ptr<net::HttpServer> graphql_http_;
  std::shared_ptr<net::WsServer> jsonrpc_ws_;
  std::shared_ptr<net::WsServer> graphql_ws_;
  std::unique_ptr<jsonrpc_server_t> jsonrpc_api_;
  std::unique_ptr<metrics::MetricsService> metrics_;

  // logging
  LOG_OBJECTS_DEFINE

  std::atomic_bool started_ = 0;

  void init();
  void close();

  /**
   * @brief Method that is used to register metrics updaters.
   * So we don't need to pass metrics classes instances in other classes.
   */
  void setupMetricsUpdaters();

 public:
  explicit FullNode(FullNodeConfig const &conf);
  ~FullNode();

  FullNode(const FullNode &) = delete;
  FullNode(FullNode &&) = delete;
  FullNode &operator=(const FullNode &) = delete;
  FullNode &operator=(FullNode &&) = delete;

  void start();
  bool isStarted() const { return started_; }
  shared_ptr_t getShared() { return shared_from_this(); }
  auto const &getConfig() const { return conf_; }
  auto const &getNetwork() const { return network_; }
  auto const &getTransactionManager() const { return trx_mgr_; }
  auto const &getDagManager() const { return dag_mgr_; }
  auto const &getDB() const { return db_; }
  auto const &getPbftManager() const { return pbft_mgr_; }
  auto const &getVoteManager() const { return vote_mgr_; }
  auto const &getNextVotesManager() const { return next_votes_mgr_; }
  auto const &getPbftChain() const { return pbft_chain_; }
  auto const &getFinalChain() const { return final_chain_; }
  // used only in tests
  auto &getDagBlockProposer() { return dag_block_proposer_; }
  auto const &getGasPricer() const { return gas_pricer_; }

  auto const &getAddress() const { return kp_.address(); }
  auto const &getPublicKey() const { return kp_.pub(); }
  auto const &getSecretKey() const { return kp_.secret(); }
  auto const &getVrfSecretKey() const { return conf_.vrf_secret; }

  // For Debug
  uint64_t getProposedBlocksCount() const;

  void rebuildDb();
};

}  // namespace taraxa
