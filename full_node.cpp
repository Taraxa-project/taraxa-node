#include "full_node.hpp"

#include <libweb3jsonrpc/JsonHelper.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <stdexcept>

#include "block_proposer.hpp"
#include "dag.hpp"
#include "dag_block.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "sortition.hpp"
#include "transaction_manager.hpp"
#include "vote.hpp"

namespace taraxa {

using std::string;
using std::to_string;

void FullNode::init(bool destroy_db, bool rebuild_network) {
  // ===== Deal with the config =====
  num_block_workers_ = conf_.dag_processing_threads;
  auto key = dev::KeyPair::create();
  if (!conf_.node_secret.empty()) {
    auto secret = dev::Secret(conf_.node_secret,
                              dev::Secret::ConstructFromStringType::FromHex);
    key = dev::KeyPair(secret);
  }
  node_sk_ = key.secret();
  node_pk_ = key.pub();
  node_addr_ = key.address();

  // Initialize logging
  for (auto &logging : conf_.log_configs) {
    setupLoggingConfiguration(node_addr_, logging);
  }

  auto &node_addr = node_addr_;
  LOG_OBJECTS_CREATE("FULLND");
  log_time_ =
      createTaraxaLogger(dev::Verbosity::VerbosityInfo, "TMSTM", node_addr_);
  log_time_dg_ =
      createTaraxaLogger(dev::Verbosity::VerbosityDebug, "TMSTM", node_addr_);

  vrf_sk_ = vrf_sk_t(conf_.vrf_secret);
  vrf_pk_ = vrf_wrapper::getVrfPublicKey(vrf_sk_);
  // Assume only first boot node is initialized
  master_boot_node_address_ =
      dev::toAddress(dev::Public(conf_.network.network_boot_nodes[0].id));
  LOG(log_si_) << "Node public key: " << EthGreen << node_pk_.toString()
               << std::endl
               << "Node address: " << EthRed << node_addr_.toString()
               << std::endl
               << "Node VRF public key: " << EthGreen << vrf_pk_.toString();
  LOG(log_nf_) << "Number of block works: " << num_block_workers_;
  // ===== Create DBs =====
  if (destroy_db) {
    boost::filesystem::remove_all(conf_.db_path);
  }
  auto const &genesis_block = conf_.chain.dag_genesis_block;
  if (!genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  auto const &genesis_hash = genesis_block.getHash();
  db_ = DbStorage::make(conf_.db_path, genesis_hash, destroy_db);
  // store genesis blk to db
  db_->saveDagBlock(genesis_block);
  LOG(log_nf_) << "DB initialized ...";
  // ===== Create services =====
  trx_mgr_ =
      std::make_shared<TransactionManager>(conf_, node_addr, db_, log_time_);
  pbft_chain_ =
      std::make_shared<PbftChain>(genesis_hash.toString(), node_addr, db_);
  dag_mgr_ = std::make_shared<DagManager>(genesis_hash.toString(), node_addr,
                                          trx_mgr_, pbft_chain_);
  blk_mgr_ = std::make_shared<BlockManager>(
      1024 /*capacity*/, 4 /* verifer thread*/, node_addr, db_, trx_mgr_,
      log_time_, conf_.test_params.max_block_queue_warn);
  trx_order_mgr_ =
      std::make_shared<TransactionOrderManager>(node_addr, db_, blk_mgr_);
  blk_proposer_ = std::make_shared<BlockProposer>(
      conf_.test_params.block_proposer, dag_mgr_, trx_mgr_, blk_mgr_,
      node_addr_, getSecretKey(), getVrfSecretKey(), log_time_);
  final_chain_ =
      NewFinalChain(db_, conf_.chain.final_chain, conf_.opts_final_chain);
  vote_mgr_ = std::make_shared<VoteManager>(node_addr, final_chain_, pbft_chain_);
  pbft_mgr_ = std::make_shared<PbftManager>(
      conf_.test_params.pbft, genesis_hash.toString(), node_addr, db_,
      pbft_chain_, vote_mgr_, dag_mgr_, blk_mgr_, final_chain_, trx_order_mgr_,
      trx_mgr_, master_boot_node_address_, node_sk_, vrf_sk_,
      conf_.opts_final_chain.state_api.ExpectedMaxNumTrxPerBlock);
  vote_mgr_->setPbftManager(pbft_mgr_);
  auto final_chain_head_ = final_chain_->get_last_block();
  trx_mgr_->setPendingBlock(
      aleth::NewPendingBlock(final_chain_head_->number(), getAddress(),
                             final_chain_head_->hash(), db_));
  if (rebuild_network) {
    network_ = std::make_shared<Network>(
        conf_.network, "", node_sk_, genesis_hash.toString(), node_addr, db_,
        pbft_chain_, vote_mgr_, dag_mgr_, blk_mgr_, trx_mgr_, node_pk_,
        conf_.test_params.pbft.lambda_ms_min);
  } else {
    network_ = std::make_shared<Network>(conf_.network, conf_.db_path + "/net",
                                         node_sk_, genesis_hash.toString(),
                                         node_addr, db_, pbft_chain_, vote_mgr_,
                                         dag_mgr_, blk_mgr_, trx_mgr_, node_pk_,
                                         conf_.test_params.pbft.lambda_ms_min);
  }
  blk_proposer_->setNetwork(network_);
  pbft_mgr_->setNetwork(network_);
  trx_mgr_->setNetwork(network_);
  // ===== Post-initialization tasks =====
  // Reconstruct DAG
  if (!destroy_db) {
    level_t level = 1;
    while (true) {
      string entry = db_->getBlocksByLevel(level);
      if (entry.empty()) break;
      vector<string> blocks;
      boost::split(blocks, entry, boost::is_any_of(","));
      for (auto const &block : blocks) {
        auto blk = db_->getDagBlock(blk_hash_t(block));
        if (blk) {
          dag_mgr_->addDagBlock(*blk);
        }
      }
      level++;
    }
    uint64_t pbft_chain_size = pbft_chain_->getPbftChainSize();
    if (pbft_chain_size) {
      // Recover DAG anchors
      dag_mgr_->recoverAnchors(pbft_chain_size);
    }
  }
  // Check pending transaction and reconstruct queues
  if (!destroy_db) {
    for (auto const &h : trx_mgr_->getPendingBlock()->transactionHashes()) {
      auto status = db_->getTransactionStatus(h);
      if (status == TransactionStatus::in_queue_unverified ||
          status == TransactionStatus::in_queue_verified) {
        auto trx = db_->getTransaction(h);
        if (!trx_mgr_->insertTrx(*trx, trx->rlp(true), true).first) {
          LOG(log_er_) << "Pending transaction not valid";
        }
      }
    }
  }
  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;
}

std::shared_ptr<FullNode> FullNode::getShared() { return shared_from_this(); }

void FullNode::start(bool boot_node) {
  // This sleep is to avoid some flaky tests failures since in our tests we
  // often start multiple nodes very quickly one after another which sometimes
  // makes nodes connect to each other at the same time where both drop each
  // other connection thinking it is a duplicate
  thisThreadSleepForMilliSeconds(10);
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  i_am_boot_node_ = boot_node;
  if (i_am_boot_node_) {
    LOG(log_nf_) << "Starting a boot node ..." << std::endl;
  }
  network_->start(boot_node);
  blk_mgr_->start();
  trx_mgr_->start();
  blk_proposer_->start();
  pbft_mgr_->start();
  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_.emplace_back([this]() {
      while (!stopped_) {
        // will block if no verified block available
        auto blk = blk_mgr_->popVerifiedBlock();
        if (dag_mgr_->dagHasVertex(blk.getHash())) {
          continue;
        }

        if (!stopped_) {
          received_blocks_++;
        }

        if (dag_mgr_->pivotAndTipsAvailable(blk)) {
          db_->saveDagBlock(blk);
          dag_mgr_->addDagBlock(blk);
          if (ws_server_) {
            ws_server_->newDagBlock(blk);
          }
          network_->onNewBlockVerified(blk);
          LOG(log_time_) << "Broadcast block " << blk.getHash()
                         << " at: " << getCurrentTimeMilliSeconds();
        } else {
          // Networking makes sure that dag block that reaches queue already had
          // its pivot and tips processed This should happen in a very rare case
          // where in some race condition older block is verfified faster then
          // new block but should resolve quickly, return block to queue
          LOG(log_wr_) << "Block could not be added to DAG " << blk.getHash();
          received_blocks_--;
          blk_mgr_->pushVerifiedBlock(blk);
        }
      }
    });
  }
  LOG(log_nf_) << "Node started ... ";
}

void FullNode::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  network_->stop();
  blk_proposer_->stop();
  blk_mgr_->stop();
  trx_mgr_->stop();
  pbft_mgr_->stop();
  trx_order_mgr_->stop();
  trx_mgr_->stop();

  for (auto &t : block_workers_) {
    t.join();
  }

  LOG(log_nf_) << "Node stopped ... ";
  for (auto &logging : conf_.log_configs) {
    removeLogging(logging);
  }
}

uint64_t FullNode::getNumReceivedBlocks() const { return received_blocks_; }

uint64_t FullNode::getNumProposedBlocks() const {
  return BlockProposer::getNumProposedBlocks();
}

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }

dev::Signature FullNode::signMessage(std::string message) {
  return dev::sign(node_sk_, dev::sha3(message));
}

bool FullNode::verifySignature(dev::Signature const &signature,
                               std::string &message) {
  return dev::verify(node_pk_, signature, dev::sha3(message));
}

}  // namespace taraxa