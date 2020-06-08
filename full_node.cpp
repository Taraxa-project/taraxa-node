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
#include "eth/util.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "sortition.hpp"
#include "transaction_manager.hpp"
#include "vote.hpp"

namespace taraxa {

using std::string;
using std::to_string;
void FullNode::setDebug(bool debug) { debug_ = debug; }

void FullNode::init(bool destroy_db, bool rebuild_network) {
  // ===== Deal with the config =====
  LOG(log_nf_) << "Read FullNode Config: " << std::endl << conf_ << std::endl;
  num_block_workers_ = conf_.dag_processing_threads;
  auto key = dev::KeyPair::create();
  if (conf_.node_secret.empty()) {
    LOG(log_si_) << "New key generated " << toHex(key.secret().ref());
  } else {
    auto secret = dev::Secret(conf_.node_secret,
                              dev::Secret::ConstructFromStringType::FromHex);
    key = dev::KeyPair(secret);
  }
  node_sk_ = key.secret();
  node_pk_ = key.pub();
  node_addr_ = key.address();
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
  LOG(log_info_) << "Number of block works: " << num_block_workers_;
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
  dag_mgr_ = std::make_shared<DagManager>(genesis_hash.toString());
  blk_mgr_ =
      std::make_shared<BlockManager>(1024 /*capacity*/, 4 /* verifer thread*/,
                                     conf_.test_params.max_block_queue_warn);
  eth_service_ = as_shared(new EthService(getShared(), conf_.chain.eth));
  trx_mgr_ =
      std::make_shared<TransactionManager>(conf_.test_params, eth_service_);
  trx_order_mgr_ = std::make_shared<TransactionOrderManager>();
  blk_proposer_ = std::make_shared<BlockProposer>(
      conf_.test_params.block_proposer, dag_mgr_->getShared(),
      trx_mgr_->getShared());
  vote_mgr_ = std::make_shared<VoteManager>();
  pbft_mgr_ = std::make_shared<PbftManager>(conf_.test_params.pbft,
                                            genesis_hash.toString());
  pbft_chain_ = std::make_shared<PbftChain>(genesis_hash.toString());
  replay_protection_service_ = std::make_shared<ReplayProtectionService>(
      conf_.chain.replay_protection_service_range, db_);
  executor_ = as_shared(
      new Executor(log_time_, db_, replay_protection_service_, eth_service_));
  if (rebuild_network) {
    network_ = std::make_shared<Network>(conf_.network, "", node_sk_,
                                         genesis_hash.toString());
  } else {
    network_ = std::make_shared<Network>(conf_.network, conf_.db_path + "/net",
                                         node_sk_, genesis_hash.toString());
  }
  // ===== Provide self to the services (link them with each other) =====
  blk_mgr_->setFullNode(getShared());
  network_->setFullNode(getShared());
  dag_mgr_->setFullNode(getShared());
  trx_mgr_->setFullNode(getShared());
  trx_order_mgr_->setFullNode(getShared());
  blk_proposer_->setFullNode(getShared());
  vote_mgr_->setFullNode(getShared());
  pbft_mgr_->setFullNode(getShared(), replay_protection_service_);
  pbft_chain_->setFullNode(getShared());
  executor_->setFullNode(getShared());
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
    for (auto const &trx : db_->getPendingTransactions()) {
      auto status = db_->getTransactionStatus(trx.first);
      if (status == TransactionStatus::in_queue_unverified ||
          status == TransactionStatus::in_queue_verified) {
        if (!trx_mgr_->insertTrx(trx.second, trx.second.rlp(true), true)
                 .first) {
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

        if (debug_) {
          std::unique_lock<std::mutex> lock(debug_mutex_);
          if (!stopped_) {
            received_blocks_++;
          }
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
}

size_t FullNode::getPeerCount() const { return network_->getPeerCount(); }
std::vector<public_t> FullNode::getAllPeers() const {
  return network_->getAllPeers();
}

void FullNode::insertBroadcastedBlockWithTransactions(
    DagBlock const &blk, std::vector<Transaction> const &transactions) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_debug_) << "Block known " << blk.getHash();
    return;
  }
  blk_mgr_->pushUnverifiedBlock(std::move(blk), std::move(transactions),
                                false /*critical*/);
  LOG(log_time_) << "Store ncblock " << blk.getHash()
                 << " at: " << getCurrentTimeMilliSeconds()
                 << " ,trxs: " << blk.getTrxs().size()
                 << " , tips: " << blk.getTips().size();
}

void FullNode::insertBlock(DagBlock const &blk) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_nf_) << "Block known " << blk.getHash();
    return;
  }
  blk_mgr_->pushUnverifiedBlock(std::move(blk), true /*critical*/);
  LOG(log_time_) << "Store cblock " << blk.getHash()
                 << " at: " << getCurrentTimeMilliSeconds()
                 << " ,trxs: " << blk.getTrxs().size()
                 << " , tips: " << blk.getTips().size();
}

bool FullNode::isBlockKnown(blk_hash_t const &hash) {
  auto known = blk_mgr_->isBlockKnown(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

std::pair<bool, std::string> FullNode::insertTransaction(Transaction const &trx,
                                                         bool verify) {
  auto rlp = trx.rlp(true);
  auto ret = trx_mgr_->insertTrx(trx, rlp, verify);
  if (ret.first && conf_.network.network_transaction_interval == 0) {
    network_->onNewTransactions({rlp});
  }
  return ret;
}

std::shared_ptr<DagBlock> FullNode::getDagBlockFromDb(
    blk_hash_t const &hash) const {
  return db_->getDagBlock(hash);
}

std::shared_ptr<DagBlock> FullNode::getDagBlock(blk_hash_t const &hash) const {
  std::shared_ptr<DagBlock> blk;
  // find if in block queue
  blk = blk_mgr_->getDagBlock(hash);
  // not in queue, search db
  if (!blk) {
    return db_->getDagBlock(hash);
  }

  return blk;
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> FullNode::getTransaction(
    trx_hash_t const &hash) const {
  if (stopped_ || !trx_mgr_) {
    return nullptr;
  }
  return trx_mgr_->getTransaction(hash);
}

unsigned long FullNode::getTransactionCount() const {
  if (stopped_ || !trx_mgr_) {
    return 0;
  }
  return trx_mgr_->getTransactionCount();
}

std::vector<std::shared_ptr<DagBlock>> FullNode::getDagBlocksAtLevel(
    level_t level, int number_of_levels) {
  std::vector<std::shared_ptr<DagBlock>> res;
  for (int i = 0; i < number_of_levels; i++) {
    if (level + i == 0) continue;  // Skip genesis
    string entry = db_->getBlocksByLevel(level + i);
    if (entry.empty()) break;
    vector<string> blocks;
    boost::split(blocks, entry, boost::is_any_of(","));
    for (auto const &block : blocks) {
      auto blk = db_->getDagBlock(blk_hash_t(block));
      if (blk) {
        res.push_back(blk);
      }
    }
  }
  return res;
}

std::vector<std::string> FullNode::collectTotalLeaves() {
  std::vector<std::string> leaves;
  dag_mgr_->collectTotalLeaves(leaves);
  return leaves;
}

void FullNode::getLatestPivotAndTips(std::string &pivot,
                                     std::vector<std::string> &tips) {
  dag_mgr_->getLatestPivotAndTips(pivot, tips);
}

void FullNode::getGhostPath(std::string const &source,
                            std::vector<std::string> &ghost) {
  dag_mgr_->getGhostPath(source, ghost);
}

void FullNode::getGhostPath(std::vector<std::string> &ghost) {
  dag_mgr_->getGhostPath(ghost);
}

std::vector<std::string> FullNode::getDagBlockPivotChain(
    blk_hash_t const &hash) {
  std::vector<std::string> pivot_chain =
      dag_mgr_->getPivotChain(hash.toString());
  return pivot_chain;
}

std::vector<std::string> FullNode::getDagBlockEpFriend(blk_hash_t const &from,
                                                       blk_hash_t const &to) {
  std::vector<std::string> epfriend =
      dag_mgr_->getEpFriendBetweenPivots(from.toString(), to.toString());
  return epfriend;
}

std::pair<bool, uint64_t> FullNode::getDagBlockPeriod(blk_hash_t const &hash) {
  return pbft_mgr_->getDagBlockPeriod(hash);
}

// return {period, block order}, for pbft-pivot-blk proposing
std::pair<uint64_t, std::shared_ptr<vec_blk_t>> FullNode::getDagBlockOrder(
    blk_hash_t const &anchor) {
  LOG(log_dg_) << "getDagBlockOrder called with anchor " << anchor;
  vec_blk_t orders;
  auto period = dag_mgr_->getDagBlockOrder(anchor, orders);
  return {period, std::make_shared<vec_blk_t>(orders)};
}
// receive pbft-povit-blk, update periods
uint FullNode::setDagBlockOrder(blk_hash_t const &anchor, uint64_t period) {
  LOG(log_dg_) << "setDagBlockOrder called with anchor " << anchor
               << " and period " << period;
  auto res = dag_mgr_->setDagBlockPeriod(anchor, period);
  if (ws_server_) ws_server_->newDagBlockFinalized(anchor, period);
  return res;
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
    auto dagblk = getDagBlock(b);
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
    auto blk = getDagBlock(dag_block_hash);
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

std::unordered_map<trx_hash_t, Transaction> FullNode::getPendingTransactions() {
  if (stopped_ || !trx_mgr_) {
    return std::unordered_map<trx_hash_t, Transaction>();
  }
  return db_->getPendingTransactions();
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

void FullNode::insertBroadcastedTransactions(
    // transactions coming from broadcastin is less critical
    std::vector<taraxa::bytes> const &transactions) {
  if (stopped_ || !trx_mgr_) {
    return;
  }
  for (auto const &t : transactions) {
    Transaction trx(t);
    trx_mgr_->insertTrx(trx, t, false);
    LOG(log_time_dg_) << "Transaction " << trx.getHash()
                      << " brkreceived at: " << getCurrentTimeMilliSeconds();
  }
}

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }
bool FullNode::isSynced() const { return network_->isSynced(); }

std::pair<val_t, bool> FullNode::getBalance(addr_t const &acc) const {
  auto state = eth_service_->getAccountsState();
  auto bal = state.balance(acc);
  if (bal == 0 && !state.addressInUse(acc)) {
    LOG(log_tr_) << "Account " << acc << " not exist ..." << std::endl;
    return {0, false};
  }
  LOG(log_tr_) << "Account " << acc << "balance: " << bal << std::endl;
  return {bal, true};
}
val_t FullNode::getMyBalance() const {
  auto my_bal = getBalance(node_addr_);
  if (!my_bal.second) {
    return 0;
  } else {
    return my_bal.first;
  }
}

addr_t FullNode::getAddress() const { return node_addr_; }

dev::Signature FullNode::signMessage(std::string message) {
  return dev::sign(node_sk_, dev::sha3(message));
}

bool FullNode::verifySignature(dev::Signature const &signature,
                               std::string &message) {
  return dev::verify(node_pk_, signature, dev::sha3(message));
}

bool FullNode::executePeriod(
    DbStorage::BatchPtr const &batch, PbftBlock const &pbft_block,
    unordered_set<addr_t> &dag_block_proposers,
    unordered_set<addr_t> &trx_senders,
    unordered_map<addr_t, val_t> &execution_touched_account_balances) {
  // update transaction overlap table first
  trx_order_mgr_->updateOrderedTrx(pbft_block.getSchedule());

  auto new_eth_header =
      executor_->execute(batch, pbft_block, dag_block_proposers, trx_senders,
                         execution_touched_account_balances);
  if (!new_eth_header) {
    return false;
  }
  if (ws_server_) {
    ws_server_->newOrderedBlock(
        dev::eth::toJson(*new_eth_header, eth_service_->sealEngine()));
  }
  return true;
}

void FullNode::updateWsScheduleBlockExecuted(PbftBlock const &pbft_block) {
  if (ws_server_) {
    ws_server_->newScheduleBlockExecuted(pbft_block);
  }
}

std::string FullNode::getScheduleBlockByPeriod(uint64_t period) {
  return pbft_mgr_->getScheduleBlockByPeriod(period);
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

uint64_t FullNode::pbftSyncingHeight() const {
  return pbft_chain_->pbftSyncingHeight();
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
                            uint64_t period, size_t step,
                            blk_hash_t const &last_pbft_block_hash) {
  // sortition proof
  VrfPbftMsg msg(last_pbft_block_hash, type, period, step);
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

void FullNode::updateNonceTable(DagBlock const &blk,
                                DagFrontier const &frontier) {
  trx_mgr_->updateNonce(blk, frontier);
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