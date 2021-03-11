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

#include "config/config.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vrf_wrapper.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/rpc/EthFace.h"
#include "network/rpc/NetFace.h"
#include "network/rpc/RpcServer.h"
#include "network/rpc/TaraxaFace.h"
#include "network/rpc/TestFace.h"
#include "network/rpc/WSServer.h"
#include "storage/db.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_order_manager.hpp"
#include "util/thread_pool.hpp"
#include "util/util.hpp"

namespace taraxa {

class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class Transaction;
class TransactionManager;
class PbftManager;
class NetworkConfig;

class FullNode : public std::enable_shared_from_this<FullNode> {
  using shared_ptr_t = std::shared_ptr<FullNode>;

  // thread pools should be destroyed last, since components may depend on them
  std::unique_ptr<util::ThreadPool> rpc_thread_pool_;
  // Has to go before shared pointers
  util::ExitStack shared_pointer_checks_;

  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  // Ethereum key pair
  dev::KeyPair kp_;
  // components
  std::shared_ptr<DB> db_;
  std::shared_ptr<DB> old_db_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<TransactionOrderManager> trx_order_mgr_;
  std::shared_ptr<BlockProposer> blk_proposer_;
  std::vector<std::thread> block_workers_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  // network API
  std::shared_ptr<net::RpcServer> jsonrpc_http_;
  std::shared_ptr<net::WSServer> jsonrpc_ws_;
  std::unique_ptr<ModularServer<net::TestFace, net::TaraxaFace, net::NetFace, net::EthFace>> jsonrpc_api_;
  // debug
  std::atomic_uint64_t received_blocks_ = 0;
  // logging
  LOG_OBJECTS_DEFINE;
  mutable logger::Logger log_time_;

  std::atomic_bool started_ = 0;

  explicit FullNode(FullNodeConfig const &conf);

  void init();
  void close();

  template <typename T>
  auto const &register_s_ptr(std::shared_ptr<T> const &ptr) {
    shared_pointer_checks_ += [w_ptr = std::weak_ptr<T>(ptr)] {
      // Example of debugging:
      //      std::cout << "checking " << typeid(T).name() << std::endl;
      assert(w_ptr.use_count() == 0);
    };
    return ptr;
  }

  template <typename T, typename... ConstructorParams>
  auto &emplace(std::shared_ptr<T> &ptr, ConstructorParams &&... ctor_params) {
    ptr = std::make_shared<T>(std::forward<ConstructorParams>(ctor_params)...);
    register_s_ptr(ptr);
    return ptr;
  }

  template <typename T, typename... ConstructorParams>
  static auto &emplace(std::unique_ptr<T> &ptr, ConstructorParams &&... ctor_params) {
    return ptr = std::make_unique<T>(std::forward<ConstructorParams>(ctor_params)...);
  }

 public:
  void start();
  bool isStarted() const { return started_; }
  auto const &getConfig() const { return conf_; }
  auto const &getNetwork() const { return network_; }
  auto const &getTransactionManager() const { return trx_mgr_; }
  auto const &getDagManager() const { return dag_mgr_; }
  auto const &getDagBlockManager() const { return dag_blk_mgr_; }
  auto const &getDB() const { return db_; }
  auto const &getPbftManager() const { return pbft_mgr_; }
  auto const &getVoteManager() const { return vote_mgr_; }
  auto const &getPbftChain() const { return pbft_chain_; }
  auto const &getFinalChain() const { return final_chain_; }

  auto const &getAddress() const { return kp_.address(); }
  auto const &getSecretKey() const { return kp_.secret(); }
  auto const &getVrfSecretKey() const { return conf_.vrf_secret; }

  // For Debug
  auto &getTimeLogger() { return log_time_; }
  auto getNumReceivedBlocks() const { return received_blocks_.load(); }
  uint64_t getNumProposedBlocks() const;

  struct Handle : shared_ptr_t {
    Handle(Handle const &) = delete;
    Handle &operator=(Handle const &) = delete;
    Handle(Handle &&) = default;
    Handle &operator=(Handle &&) = default;
    Handle() = default;
    explicit Handle(FullNodeConfig const &conf, bool start = false);
    ~Handle();
  };

  void rebuildDb();
};

}  // namespace taraxa
