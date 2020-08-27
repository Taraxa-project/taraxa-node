#ifndef TARAXA_NODE_FULL_NODE_HPP
#define TARAXA_NODE_FULL_NODE_HPP

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

#include "aleth/filter_api.hpp"
#include "aleth/pending_block.hpp"
#include "config.hpp"
#include "db_storage.hpp"
#include "net/WSServer.h"
#include "pbft_chain.hpp"
#include "transaction.hpp"
#include "transaction_order_manager.hpp"
#include "util.hpp"
#include "vote.hpp"
#include "vrf_wrapper.hpp"

class Top;

namespace taraxa {

class RocksDb;
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
  friend class ::Top;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using vrf_output_t = vrf_wrapper::vrf_output_t;

  explicit FullNode(FullNodeConfig const &conf) : conf_(conf) {}
  explicit FullNode(std::string const &conf) : FullNode(FullNodeConfig(conf)) {}

  void init(bool destroy_db, bool rebuild_network);
  void stop();

 public:
  using shared_ptr_t = std::shared_ptr<FullNode>;

  struct Handle : shared_ptr_t {
    explicit Handle(FullNode *ptr) : shared_ptr_t(ptr) {}
    // default-constructible
    Handle() = default;
    // not copyable
    Handle(Handle const &) = delete;
    Handle &operator=(Handle const &) = delete;
    // movable
    Handle(Handle &&) = default;
    Handle &operator=(Handle &&) = default;

    ~Handle() {
      auto node = this->get();
      if (!node) {
        return;
      }
      node->stop();

      // Verify that all of the modules are destroyed once fullNode object is
      // destroyed
      std::weak_ptr<DbStorage> db = node->getDB();
      std::weak_ptr<BlockManager> blockManager = node->getBlockManager();
      std::weak_ptr<DagManager> dagManager = node->getDagManager();
      std::weak_ptr<Network> network = node->getNetwork();
      std::weak_ptr<PbftChain> pbftChain = node->getPbftChain();
      std::weak_ptr<PbftManager> pbftManager = node->getPbftManager();
      std::weak_ptr<TransactionManager> transactionManager =
          node->getTransactionManager();

      this->reset();

      assert(db.use_count() == 0);
      assert(blockManager.use_count() == 0);
      assert(dagManager.use_count() == 0);
      assert(network.use_count() == 0);
      assert(pbftChain.use_count() == 0);
      assert(pbftManager.use_count() == 0);
      assert(transactionManager.use_count() == 0);

      assert(this->use_count() == 0);
    }
  };

  template <typename Config>
  static Handle make(Config const &config,
                     bool destroy_db = false,  //
                     bool rebuild_network = false) {
    Handle ret(new FullNode(config));
    ret->init(destroy_db, rebuild_network);
    return ret;
  }

  void start(bool boot_node);
  // ** Note can be called only FullNode is fully settled!!!
  std::shared_ptr<FullNode> getShared();

  FullNodeConfig const &getConfig() const;
  std::shared_ptr<Network> getNetwork() const;
  std::shared_ptr<TransactionManager> getTransactionManager() const {
    return trx_mgr_;
  }
  std::shared_ptr<DagManager> getDagManager() const { return dag_mgr_; }
  std::shared_ptr<BlockManager> getBlockManager() const { return blk_mgr_; }

  // master boot node
  addr_t getMasterBootNodeAddress() const { return master_boot_node_address_; }

  auto const &getAddress() const { return node_addr_; }
  public_t getPublicKey() const { return node_pk_; }
  auto const &getSecretKey() const { return node_sk_; }
  auto getVrfSecretKey() const { return vrf_sk_; }
  auto getVrfPublicKey() const { return vrf_pk_; }

  std::shared_ptr<DbStorage> getDB() const { return db_; }

  // PBFT
  dev::Signature signMessage(std::string message);
  bool verifySignature(dev::Signature const &signature, std::string &message);
  std::vector<Vote> getAllVotes();
  uint64_t getUnverifiedVotesSize() const;
  boost::log::sources::severity_channel_logger<> &getTimeLogger() { return log_time_; }
  std::shared_ptr<PbftManager> getPbftManager() const { return pbft_mgr_; }

  std::shared_ptr<VoteManager> getVoteManager() const { return vote_mgr_; }
  std::shared_ptr<PbftChain> getPbftChain() const { return pbft_chain_; }

  // For Debug
  uint64_t getNumReceivedBlocks() const;
  uint64_t getNumProposedBlocks() const;

  void setWSServer(std::shared_ptr<taraxa::net::WSServer> const &ws_server);
  auto getFinalChain() const { return final_chain_; }
  auto getTrxOrderMgr() const { return trx_order_mgr_; }
  auto getWSServer() const { return ws_server_; }

 private:
  size_t num_block_workers_ = 2;
  // fixme (here and everywhere else): non-volatile
  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  uint64_t propose_threshold_ = 512;
  bool i_am_boot_node_ = false;
  // node secrets
  secret_t node_sk_;
  public_t node_pk_;
  addr_t node_addr_;
  vrf_sk_t vrf_sk_;
  vrf_pk_t vrf_pk_;
  addr_t master_boot_node_address_;

  // network
  std::shared_ptr<Network> network_;
  // dag
  std::shared_ptr<DagManager> dag_mgr_;

  // ledger
  std::shared_ptr<BlockManager> blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<TransactionOrderManager> trx_order_mgr_;
  // block proposer (multi processing)
  std::shared_ptr<BlockProposer> blk_proposer_;
  //
  std::vector<std::thread> block_workers_;
  // PBFT
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  //
  std::shared_ptr<FinalChain> final_chain_;
  //
  std::shared_ptr<taraxa::net::WSServer> ws_server_;
  // storage
  std::shared_ptr<DbStorage> db_ = nullptr;

  // debugger
  std::atomic_uint64_t received_blocks_ = 0;
  LOG_OBJECTS_DEFINE;
  mutable taraxa::Logger log_time_;
  mutable taraxa::Logger log_time_dg_;
};

}  // namespace taraxa
#endif
