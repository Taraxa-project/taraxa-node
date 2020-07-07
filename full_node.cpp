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
  dag_mgr_ = std::make_shared<DagManager>(genesis_hash.toString(), node_addr);
  blk_mgr_ = std::make_shared<BlockManager>(
      1024 /*capacity*/, 4 /* verifer thread*/, node_addr,
      conf_.test_params.max_block_queue_warn);
  trx_mgr_ = std::make_shared<TransactionManager>(conf_, node_addr);
  trx_order_mgr_ = std::make_shared<TransactionOrderManager>(node_addr);
  blk_proposer_ = std::make_shared<BlockProposer>(
      conf_.test_params.block_proposer, dag_mgr_->getShared(),
      trx_mgr_->getShared(), network_, node_addr_);
  vote_mgr_ = std::make_shared<VoteManager>(node_addr);
  pbft_mgr_ = std::make_shared<PbftManager>(conf_.test_params.pbft,
                                            genesis_hash.toString(), node_addr);
  pbft_chain_ = std::make_shared<PbftChain>(genesis_hash.toString(), node_addr);
  final_chain_ =
      NewFinalChain(db_, conf_.chain.final_chain, conf_.opts_final_chain);
  auto final_chain_head_ = final_chain_->get_last_block();
  pending_block_ =
      aleth::NewPendingBlock(final_chain_head_->number(), getAddress(),
                             final_chain_head_->hash(), db_);
  filter_api_ = aleth::NewFilterAPI();
  trx_mgr_->event_transaction_accepted.sub([=](auto const &h) {
    pending_block_->add_transactions(vector{h});
    filter_api_->note_pending_transactions(vector{h});
  });
  if (rebuild_network) {
    network_ = std::make_shared<Network>(conf_.network, "", node_sk_,
                                         genesis_hash.toString(), node_addr);
  } else {
    network_ =
        std::make_shared<Network>(conf_.network, conf_.db_path + "/net",
                                  node_sk_, genesis_hash.toString(), node_addr);
  }
  // ===== Provide self to the services (link them with each other) =====
  blk_mgr_->setFullNode(getShared());
  network_->setFullNode(getShared());
  dag_mgr_->setFullNode(getShared());
  trx_mgr_->setFullNode(getShared());
  trx_order_mgr_->setFullNode(getShared());
  blk_proposer_->setFullNode(getShared());
  vote_mgr_->setFullNode(getShared());
  pbft_mgr_->setFullNode(getShared());
  pbft_chain_->setFullNode(getShared());
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
    for (auto const &h : pending_block_->transactionHashes()) {
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
  for (auto &t : block_workers_) {
    t.join();
  }

  LOG(log_nf_) << "Node stopped ... ";
  for (auto &logging : conf_.log_configs) {
    removeLogging(logging);
  }
}

uint64_t FullNode::getLatestPeriod() const {
  return dag_mgr_->getLatestPeriod();
}
blk_hash_t FullNode::getLatestAnchor() const {
  return blk_hash_t(dag_mgr_->getLatestAnchor());
}

std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
FullNode::computeTransactionOverlapTable(
    std::shared_ptr<vec_blk_t> ordered_dag_blocks) {
  std::vector<std::shared_ptr<DagBlock>> blks;
  for (auto const &b : *ordered_dag_blocks) {
    auto dagblk = blk_mgr_->getDagBlock(b);
    assert(dagblk);
    blks.emplace_back(dagblk);
  }
  return trx_order_mgr_->computeOrderInBlocks(blks);
}

std::vector<std::vector<uint>> FullNode::createMockTrxSchedule(
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table) {
  std::vector<std::vector<uint>> blocks_trx_modes;

  if (!trx_overlap_table) {
    LOG(log_er_) << "Transaction overlap table nullptr, cannot create mock "
                 << "transactions schedule";
    return blocks_trx_modes;
  }

  for (auto i = 0; i < trx_overlap_table->size(); i++) {
    blk_hash_t &dag_block_hash = (*trx_overlap_table)[i].first;
    auto blk = blk_mgr_->getDagBlock(dag_block_hash);
    if (!blk) {
      LOG(log_er_) << "Cannot create schedule block, DAG block missing "
                   << dag_block_hash;
      continue;
    }

    auto num_trx = blk->getTrxs().size();
    std::vector<uint> block_trx_modes;
    for (auto j = 0; j < num_trx; j++) {
      if ((*trx_overlap_table)[i].second[j]) {
        // trx sequential mode
        block_trx_modes.emplace_back(1);
      } else {
        // trx invalid mode
        block_trx_modes.emplace_back(0);
      }
    }
    blocks_trx_modes.emplace_back(block_trx_modes);
  }

  return blocks_trx_modes;
}

uint64_t FullNode::getNumReceivedBlocks() const { return received_blocks_; }

uint64_t FullNode::getNumProposedBlocks() const {
  return BlockProposer::getNumProposedBlocks();
}

std::pair<uint64_t, uint64_t> FullNode::getNumVerticesInDag() const {
  return dag_mgr_->getNumVerticesInDag();
}

std::pair<uint64_t, uint64_t> FullNode::getNumEdgesInDag() const {
  return dag_mgr_->getNumEdgesInDag();
}

void FullNode::drawGraph(std::string const &dotfile) const {
  dag_mgr_->drawPivotGraph("pivot." + dotfile);
  dag_mgr_->drawTotalGraph("total." + dotfile);
}

std::unordered_map<trx_hash_t, Transaction> FullNode::getVerifiedTrxSnapShot() {
  if (stopped_ || !trx_mgr_) {
    return std::unordered_map<trx_hash_t, Transaction>();
  }
  return trx_mgr_->getVerifiedTrxSnapShot();
}

std::vector<taraxa::bytes> FullNode::getNewVerifiedTrxSnapShotSerialized() {
  if (stopped_ || !trx_mgr_) {
    return {};
  }
  return trx_mgr_->getNewVerifiedTrxSnapShotSerialized();
}

std::pair<size_t, size_t> FullNode::getTransactionQueueSize() const {
  if (stopped_ || !trx_mgr_) {
    return std::pair<size_t, size_t>();
  }
  return trx_mgr_->getTransactionQueueSize();
}

std::pair<size_t, size_t> FullNode::getDagBlockQueueSize() const {
  if (stopped_ || !blk_mgr_) {
    return std::pair<size_t, size_t>();
  }
  return blk_mgr_->getDagBlockQueueSize();
}

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }

std::pair<val_t, bool> FullNode::getBalance(addr_t const &addr) const {
  if (auto acc = final_chain_->get_account(addr)) {
    LOG(log_tr_) << "Account " << addr << "balance: " << acc->Balance
                 << std::endl;
    return {acc->Balance, true};
  }
  LOG(log_tr_) << "Account " << addr << " not exist ..." << std::endl;
  return {0, false};
}
val_t FullNode::getMyBalance() const {
  auto my_bal = getBalance(node_addr_);
  if (!my_bal.second) {
    return 0;
  } else {
    return my_bal.first;
  }
}

dev::Signature FullNode::signMessage(std::string message) {
  return dev::sign(node_sk_, dev::sha3(message));
}

bool FullNode::verifySignature(dev::Signature const &signature,
                               std::string &message) {
  return dev::verify(node_pk_, signature, dev::sha3(message));
}

void FullNode::updateWsPbftBlockExecuted(PbftBlock const &pbft_block) {
  if (ws_server_) {
    ws_server_->newPbftBlockExecuted(pbft_block);
  }
}

std::vector<Vote> FullNode::getAllVotes() { return vote_mgr_->getAllVotes(); }

bool FullNode::addVote(taraxa::Vote const &vote) {
  return vote_mgr_->addVote(vote);
}

void FullNode::broadcastVote(Vote const &vote) {
  // come from RPC
  network_->onNewPbftVote(vote);
}

bool FullNode::shouldSpeak(PbftVoteTypes type, uint64_t period, size_t step) {
  return pbft_mgr_->shouldSpeak(type, period, step);
}

void FullNode::clearUnverifiedVotesTable() {
  vote_mgr_->clearUnverifiedVotesTable();
}

uint64_t FullNode::getUnverifiedVotesSize() const {
  return vote_mgr_->getUnverifiedVotesSize();
}

bool FullNode::isKnownPbftBlockForSyncing(
    taraxa::blk_hash_t const &pbft_block_hash) const {
  return pbft_chain_->findPbftBlockInSyncedSet(pbft_block_hash) ||
         pbft_chain_->findPbftBlockInChain(pbft_block_hash);
}

bool FullNode::isKnownUnverifiedPbftBlock(
    taraxa::blk_hash_t const &pbft_block_hash) const {
  return pbft_chain_->findUnverifiedPbftBlock(pbft_block_hash);
}

void FullNode::pushUnverifiedPbftBlock(taraxa::PbftBlock const &pbft_block) {
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
}

uint64_t FullNode::pbftSyncingPeriod() const {
  return pbft_chain_->pbftSyncingPeriod();
}

uint64_t FullNode::getPbftChainSize() const {
  return pbft_chain_->getPbftChainSize();
}

void FullNode::newPendingTransaction(trx_hash_t const &trx_hash) {
  if (ws_server_) ws_server_->newPendingTransaction(trx_hash);
}

void FullNode::setSyncedPbftBlock(PbftBlockCert const &pbft_block_and_votes) {
  pbft_chain_->setSyncedPbftBlockIntoQueue(pbft_block_and_votes);
}

Vote FullNode::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type,
                            uint64_t round, size_t step,
                            blk_hash_t const &last_pbft_block_hash) {
  // sortition proof
  VrfPbftMsg msg(last_pbft_block_hash, type, round, step);
  VrfPbftSortition vrf_sortition(vrf_sk_, msg);
  Vote vote(node_sk_, vrf_sortition, blockhash);

  LOG(log_dg_) << "last pbft block hash " << last_pbft_block_hash
               << " vote: " << vote.getHash();
  return vote;
}

level_t FullNode::getMaxDagLevel() const { return dag_mgr_->getMaxLevel(); }

level_t FullNode::getMaxDagLevelInQueue() const {
  return std::max(dag_mgr_->getMaxLevel(), blk_mgr_->getMaxDagLevelInQueue());
}

// Need remove later, keep it now for reuse
bool FullNode::pbftBlockHasEnoughCertVotes(blk_hash_t const &blk_hash,
                                           std::vector<Vote> &votes) const {
  std::vector<Vote> valid_votes;
  for (auto const &v : votes) {
    if (v.getType() != cert_vote_type) {
      continue;
    } else if (v.getStep() != 3) {
      continue;
    } else if (v.getBlockHash() != blk_hash) {
      continue;
    }
    blk_hash_t pbft_chain_last_block_hash = pbft_chain_->getLastPbftBlockHash();
    size_t valid_sortition_players =
        pbft_mgr_->sortition_account_balance_table.size();
    size_t sortition_threshold = pbft_mgr_->getSortitionThreshold();
    if (vote_mgr_->voteValidation(pbft_chain_last_block_hash, v,
                                  valid_sortition_players,
                                  sortition_threshold)) {
      valid_votes.emplace_back(v);
    }
  }
  return valid_votes.size() >= pbft_mgr_->getTwoTPlusOne();
}

void FullNode::setTwoTPlusOne(size_t val) { pbft_mgr_->setTwoTPlusOne(val); }

bool FullNode::checkPbftBlockValidationFromSyncing(
    PbftBlock const &pbft_block) const {
  return pbft_chain_->checkPbftBlockValidationFromSyncing(pbft_block);
}

size_t FullNode::getPbftSyncedQueueSize() const {
  return pbft_chain_->pbftSyncedQueueSize();
}

std::unordered_set<std::string> FullNode::getUnOrderedDagBlks() const {
  return dag_mgr_->getUnOrderedDagBlks();
}

}  // namespace taraxa