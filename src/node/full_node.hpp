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
#include "consensus/pbft_chain.hpp"
#include "consensus/vote.hpp"
#include "consensus/vrf_wrapper.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/rpc/EthFace.h"
#include "network/rpc/NetFace.h"
#include "network/rpc/RpcServer.h"
#include "network/rpc/TaraxaFace.h"
#include "network/rpc/TestFace.h"
#include "network/rpc/WSServer.h"
#include "storage/db_storage.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_order_manager.hpp"
#include "util/thread_pool.hpp"
#include "util/util.hpp"

namespace taraxa {

class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class BlockManager;
struct Transaction;
class TransactionManager;
class PbftManager;
struct NetworkConfig;

class FullNode : public std::enable_shared_from_this<FullNode> {
  using shared_ptr_t = std::shared_ptr<FullNode>;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;

  struct PostDestructionContext {
    uint num_shared_pointers_to_check = 0;
    bool have_leaked_shared_pointers = false;
  };
  std::shared_ptr<PostDestructionContext> post_destruction_ctx_;
  // Has to be destroyed last, hence on top
  util::ExitStack post_destruction_;

  // should be destroyed after all components, since they may depend on it through unsafe pointers
  std::unique_ptr<util::ThreadPool> rpc_thread_pool_;

  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  // Ethereum key pair
  dev::KeyPair kp_;
  // components
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DbStorage> old_db_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<TransactionOrderManager> trx_order_mgr_;
  std::shared_ptr<BlockProposer> blk_proposer_;
  std::vector<std::thread> block_workers_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<net::RpcServer> jsonrpc_http_;
  std::shared_ptr<net::WSServer> jsonrpc_ws_;
  std::unique_ptr<ModularServer<net::TestFace, net::TaraxaFace, net::NetFace, net::EthFace>> jsonrpc_api_;
  // debug
  std::atomic_uint64_t received_blocks_ = 0;
  // logging
  LOG_OBJECTS_DEFINE
  mutable logger::Logger log_time_;

  std::atomic_bool started_ = 0;

  explicit FullNode(FullNodeConfig const &conf);

  void init();
  void close();

  template <typename T>
  auto const &register_s_ptr(std::shared_ptr<T> const &ptr) {
    ++post_destruction_ctx_->num_shared_pointers_to_check;
    post_destruction_ += [w_ptr = std::weak_ptr<T>(ptr), ctx = post_destruction_ctx_] {
      if (w_ptr.use_count() != 0) {
        cerr << "in taraxa::FullNode a shared pointer to type " << typeid(T).name() << " has not been released "
             << endl;
        ctx->have_leaked_shared_pointers = true;
      }
      if (--ctx->num_shared_pointers_to_check == 0 && ctx->have_leaked_shared_pointers) {
        assert(false);
        exit(1);
      }
    };
    return ptr;
  }

  template <typename T, typename... ConstructorParams>
  auto &emplace(std::shared_ptr<T> &ptr, ConstructorParams &&...ctor_params) {
    ptr = std::make_shared<T>(std::forward<ConstructorParams>(ctor_params)...);
    register_s_ptr(ptr);
    return ptr;
  }

  template <typename T, typename... ConstructorParams>
  static auto &emplace(std::unique_ptr<T> &ptr, ConstructorParams &&...ctor_params) {
    return ptr = std::make_unique<T>(std::forward<ConstructorParams>(ctor_params)...);
  }

 public:
  void start();
  bool isStarted() const { return started_; }
  shared_ptr_t getShared() { return shared_from_this(); }
  auto const &getConfig() const { return conf_; }
  auto const &getNetwork() const { return network_; }
  auto const &getTransactionManager() const { return trx_mgr_; }
  auto const &getDagManager() const { return dag_mgr_; }
  auto const &getDagBlockManager() const { return dag_blk_mgr_; }
  auto const &getDB() const { return db_; }
  auto const &getPbftManager() const { return pbft_mgr_; }
  auto const &getVoteManager() const { return vote_mgr_; }
  auto const &getNextVotesManager() const { return next_votes_mgr_; }
  auto const &getPbftChain() const { return pbft_chain_; }
  auto const &getFinalChain() const { return final_chain_; }
  auto const &getTrxOrderMgr() const { return trx_order_mgr_; }

  auto const &getAddress() const { return kp_.address(); }
  auto const &getPublicKey() const { return kp_.pub(); }
  auto const &getSecretKey() const { return kp_.secret(); }
  auto const &getVrfSecretKey() const { return conf_.vrf_secret; }

  // For Debug
  auto &getTimeLogger() { return log_time_; }
  auto getNumReceivedBlocks() const { return received_blocks_.load(); }
  uint64_t getNumProposedBlocks() const;

  // PBFT
  dev::Signature signMessage(std::string const &message);

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

  static constexpr uint16_t c_node_major_version = 1;
  static constexpr uint16_t c_node_minor_version = 8;

  // Any time a change in the network protocol is introduced this version should be increased
  static constexpr uint16_t c_network_protocol_version = 10;

  // Major version is modified when DAG blocks, pbft blocks and any basic building blocks of our blockchan is modified
  // in the db
  static constexpr uint16_t c_database_major_version = 1;
  // Minor version should be modified when changes to the database are made in the tables that can be rebuilt from the
  // basic tables
  static constexpr uint16_t c_database_minor_version = 2;
};

}  // namespace taraxa
