#ifndef TARAXA_NODE_FULL_NODE_HPP
#define TARAXA_NODE_FULL_NODE_HPP

#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libweb3jsonrpc/EthFace.h>

#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "aleth/filter_api.hpp"
#include "aleth/pending_block.hpp"
#include "config.hpp"
#include "db_storage.hpp"
#include "net/NetFace.h"
#include "net/RpcServer.h"
#include "net/TaraxaFace.h"
#include "net/TestFace.h"
#include "net/WSServer.h"
#include "pbft_chain.hpp"
#include "transaction.hpp"
#include "transaction_order_manager.hpp"
#include "util.hpp"
#include "vote.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {

class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class BlockManager;
class Transaction;
class TransactionManager;
class PbftManager;
class NetworkConfig;

class FullNode : public std::enable_shared_from_this<FullNode> {
  using shared_ptr_t = std::shared_ptr<FullNode>;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;

  // Has to be destroyed last, hence on top
  util::ExitStack post_destruction_;

  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  uint64_t propose_threshold_ = 512;
  // Ethereum key pair
  dev::KeyPair kp_;
  // components
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<BlockManager> blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<TransactionOrderManager> trx_order_mgr_;
  std::shared_ptr<BlockProposer> blk_proposer_;
  std::vector<std::thread> block_workers_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::unique_ptr<boost::asio::io_context> jsonrpc_io_ctx_;
  std::shared_ptr<net::RpcServer> jsonrpc_http_;
  std::shared_ptr<net::WSServer> jsonrpc_ws_;
  std::unique_ptr<ModularServer<net::TestFace,
                                net::TaraxaFace,  //
                                net::NetFace,     //
                                dev::rpc::EthFace>>
      jsonrpc_api_;
  std::unique_ptr<std::thread> jsonrpc_thread_;
  // debug
  std::atomic_uint64_t received_blocks_ = 0;
  // logging
  LOG_OBJECTS_DEFINE;
  mutable taraxa::Logger log_time_;

  explicit FullNode(FullNodeConfig const &conf);

  void init();
  void close();

  template <typename T, typename... ConstructorParams>
  auto &emplace(std::shared_ptr<T> &ptr, ConstructorParams &&... ctor_params) {
    ptr = std::make_shared<T>(std::forward<ConstructorParams>(ctor_params)...);
    post_destruction_ += [w_ptr = std::weak_ptr<T>(ptr)] {
      // Example of debugging: cout << "checking " << typeid(T).name() << endl;
      assert(w_ptr.use_count() == 0);
    };
    return ptr;
  }

  template <typename T, typename... ConstructorParams>
  static auto &emplace(std::unique_ptr<T> &ptr, ConstructorParams &&... ctor_params) {
    return ptr = std::make_unique<T>(std::forward<ConstructorParams>(ctor_params)...);
  }

 public:
  void start();
  shared_ptr_t getShared() { return shared_from_this(); }
  auto const &getConfig() const { return conf_; }
  auto const &getNetwork() const { return network_; }
  auto const &getTransactionManager() const { return trx_mgr_; }
  auto const &getDagManager() const { return dag_mgr_; }
  auto const &getBlockManager() const { return blk_mgr_; }
  auto const &getDB() const { return db_; }
  auto const &getPbftManager() const { return pbft_mgr_; }
  auto const &getVoteManager() const { return vote_mgr_; }
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
};

}  // namespace taraxa

#endif
