/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-01 15:43:56
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-16 12:54:43
 */
#include "full_node.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include "SimpleDBFactory.h"
#include "block_proposer.hpp"
#include "dag.hpp"
#include "dag_block.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "vote.h"

namespace taraxa {

using std::string;
using std::to_string;

void FullNode::setVerbose(bool verbose) {
  verbose_ = verbose;
  dag_mgr_->setVerbose(verbose);
  // TODO: setup logs for DB
  // db_blks_->setVerbose(verbose);
}

void FullNode::setDebug(bool debug) { debug_ = debug; }

FullNode::FullNode(boost::asio::io_context &io_context,
                   std::string const &conf_full_node_file, bool destroy_db)
    : FullNode(io_context, FullNodeConfig(conf_full_node_file), destroy_db) {}
FullNode::FullNode(boost::asio::io_context &io_context,
                   FullNodeConfig const &conf_full_node, bool destroy_db) try
    : io_context_(io_context),
      num_block_workers_(conf_full_node.dag_processing_threads),
      conf_(conf_full_node),
      db_accs_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::StateDBKind, conf_.db_path + "/acc",
          destroy_db)),
      db_blks_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::OverlayDBKind, conf_.db_path + "/blk",
          destroy_db)),
      db_blks_index_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::TaraxaRocksDBKind,
          conf_.db_path + "/blk_index", destroy_db)),
      db_trxs_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::OverlayDBKind, conf_.db_path + "/trx",
          destroy_db)),
      blk_mgr_(std::make_shared<BlockManager>(1024 /*capacity*/,
                                              4 /* verifer thread*/)),
      trx_mgr_(std::make_shared<TransactionManager>(db_trxs_)),
      dag_mgr_(std::make_shared<DagManager>()),
      blk_proposer_(std::make_shared<BlockProposer>(conf_.proposer,
                                                    dag_mgr_->getShared(),
                                                    trx_mgr_->getShared())),
      executor_(std::make_shared<Executor>(db_blks_, db_trxs_, db_accs_)),
      pbft_mgr_(std::make_shared<PbftManager>(conf_full_node.pbft_manager)),
      vote_queue_(std::make_shared<VoteQueue>()),
      pbft_chain_(std::make_shared<PbftChain>()),
      db_votes_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::OverlayDBKind,
          conf_.db_path + "/pbftvotes", destroy_db)),
      db_pbftchain_(SimpleDBFactory::createDelegate(
          SimpleDBFactory::SimpleDBType::OverlayDBKind,
          conf_.db_path + "/pbftchain", destroy_db)) {
  LOG(log_nf_) << "Read FullNode Config: " << std::endl << conf_ << std::endl;

  auto key = dev::KeyPair::create();
  if (conf_.node_secret.empty()) {
    LOG(log_si_) << "New key generated " << toHex(key.secret().ref());
  } else {
    auto secret = dev::Secret(conf_.node_secret,
                              dev::Secret::ConstructFromStringType::FromHex);
    key = dev::KeyPair(secret);
  }
  network_ =
      std::make_shared<Network>(conf_full_node.network, "", key.secret());
  node_sk_ = key.secret();
  node_pk_ = key.pub();
  node_addr_ = key.address();
  DagBlock genesis;
  // store genesis blk to db
  db_blks_->put(genesis.getHash().toString(), genesis.getJsonStr());
  db_blks_->commit();
  // store pbft chain genesis(HEAD) block to db
  db_pbftchain_->put(pbft_chain_->getGenesisHash().toString(),
                     pbft_chain_->getJsonStr());
  db_pbftchain_->commit();

  if (!destroy_db) {
    unsigned long level = 1;
    while (true) {
      string entry = db_blks_index_->get(std::to_string(level));
      if (entry.empty()) break;
      vector<string> blocks;
      boost::split(blocks, entry, boost::is_any_of(","));
      for (auto const &block : blocks) {
        auto block_json = db_blks_->get(block);
        if (block_json != "") dag_mgr_->addDagBlock(DagBlock(block_json));
      }
      level++;
    }
  }

  LOG(log_si_) << "Node public key: " << EthGreen << node_pk_.toString()
               << std::endl;
  LOG(log_si_) << "Node address: " << EthRed << node_addr_.toString()
               << std::endl;
  LOG(log_si_) << "Number of block works: " << num_block_workers_;
  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;

} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  throw e;
}

std::shared_ptr<FullNode> FullNode::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "FullNode: " << e.what() << std::endl;
    return nullptr;
  }
}
boost::asio::io_context &FullNode::getIoContext() { return io_context_; }

void FullNode::start(bool boot_node) {
  if (!stopped_) {
    return;
  }
  stopped_ = false;
  network_->setFullNode(getShared());
  network_->start(boot_node);
  dag_mgr_->start();
  blk_mgr_->setFullNode(getShared());
  blk_mgr_->start();
  blk_proposer_->setFullNode(getShared());
  blk_proposer_->start();
  trx_mgr_->start();
  pbft_mgr_->setFullNode(getShared());
  pbft_mgr_->start();
  executor_->setFullNode(getShared());
  executor_->start();
  if (boot_node) {
    LOG(log_nf_) << "Starting a boot node ..." << std::endl;
  }

  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_.emplace_back([this]() {
      while (!stopped_) {
        // will block if no verified block available
        auto blk = blk_mgr_->popVerifiedBlock();
        if (stopped_) break;

        if (debug_) {
          std::unique_lock<std::mutex> lock(debug_mutex_);
          if (!stopped_) {
            received_blocks_++;
          }
        }

        dag_mgr_->addDagBlock(blk);
        {
          db_blks_->put(blk.getHash().toString(), blk.getJsonStr());
          db_blks_->commit();
          std::string level = std::to_string(blk.getLevel());
          std::string blocks = db_blks_index_->get(level);
          if (blocks == "")
            db_blks_index_->put(level, blk.getHash().hex());
          else
            db_blks_index_->update(level, blocks + "," + blk.getHash().hex());
          db_blks_index_->commit();
        }
        network_->onNewBlockVerified(blk);
        LOG(log_time_) << "Broadcast block " << blk.getHash()
                       << " at: " << getCurrentTimeMilliSeconds();
      }
    });
  }
}

void FullNode::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  dag_mgr_->stop();  // dag_mgr_ stopped, notify blk_proposer ...
  blk_proposer_->stop();
  blk_mgr_->stop();
  network_->stop();
  trx_mgr_->stop();
  pbft_mgr_->stop();
  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_[i].join();
  }
}

size_t FullNode::getPeerCount() const { return network_->getPeerCount(); }
std::vector<public_t> FullNode::getAllPeers() const {
  return network_->getAllPeers();
}

void FullNode::insertBroadcastedBlockWithTransactions(
    DagBlock const &blk, std::vector<Transaction> const &transactions) {
  blk_mgr_->pushUnverifiedBlock(std::move(blk), std::move(transactions),
                                false /*critical*/);
  LOG(log_time_) << "Store ncblock " << blk.getHash()
                 << " at: " << getCurrentTimeMilliSeconds();
}

void FullNode::insertBlock(DagBlock const &blk) {
  blk_mgr_->pushUnverifiedBlock(std::move(blk), true /*critical*/);
  LOG(log_time_) << "Store cblock " << blk.getHash()
                 << " at: " << getCurrentTimeMilliSeconds();
}

void FullNode::insertBlockAndSign(DagBlock const &blk) {
  DagBlock sign_block(blk);
  sign_block.sign(node_sk_);

  auto now = getCurrentTimeMilliSeconds();

  LOG(log_time_) << "Propose block " << sign_block.getHash() << " at: " << now
                 << " ,trxs: " << sign_block.getTrxs();

  insertBlock(sign_block);
}

bool FullNode::isBlockKnown(blk_hash_t const &hash) {
  auto known = blk_mgr_->isBlockKnown(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

void FullNode::insertTransaction(Transaction const &trx) {
  if (conf_.network.network_transaction_interval == 0) {
    std::unordered_map<trx_hash_t, Transaction> map_trx;
    map_trx[trx.getHash()] = trx;
    network_->onNewTransactions(map_trx);
  }
  trx_mgr_->insertTrx(trx, true);
}

std::shared_ptr<DagBlock> FullNode::getDagBlockFromDb(blk_hash_t const &hash) {
  std::string json = db_blks_->get(hash.toString());
  if (!json.empty()) {
    return std::make_shared<DagBlock>(json);
  }
  return nullptr;
}

std::shared_ptr<DagBlock> FullNode::getDagBlock(blk_hash_t const &hash) {
  std::shared_ptr<DagBlock> blk;
  // find if in block queue
  blk = blk_mgr_->getDagBlock(hash);
  // not in queue, search db
  if (!blk) {
    std::string json = db_blks_->get(hash.toString());
    if (!json.empty()) {
      blk = std::make_shared<DagBlock>(json);
    }
  }

  return blk;
}

std::shared_ptr<Transaction> FullNode::getTransaction(trx_hash_t const &hash) {
  return trx_mgr_->getTransaction(hash);
}

unsigned long FullNode::getTransactionStatusCount() {
  return trx_mgr_->getTransactionStatusCount();
}

time_stamp_t FullNode::getDagBlockTimeStamp(blk_hash_t const &hash) {
  return dag_mgr_->getDagBlockTimeStamp(hash.toString());
}

void FullNode::setDagBlockTimeStamp(blk_hash_t const &hash,
                                    time_stamp_t stamp) {
  dag_mgr_->setDagBlockTimeStamp(hash.toString(), stamp);
}

std::vector<std::string> FullNode::getDagBlockChildren(blk_hash_t const &hash,
                                                       time_stamp_t stamp) {
  std::vector<std::string> children =
      dag_mgr_->getPivotChildrenBeforeTimeStamp(hash.toString(), stamp);
  return children;
}

std::vector<std::string> FullNode::getTotalDagBlockChildren(
    blk_hash_t const &hash, time_stamp_t stamp) {
  std::vector<std::string> children =
      dag_mgr_->getTotalChildrenBeforeTimeStamp(hash.toString(), stamp);
  return children;
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

// Recursive call to children
std::vector<std::string> FullNode::getDagBlockSubtree(blk_hash_t const &hash,
                                                      time_stamp_t stamp) {
  std::vector<std::string> subtree =
      dag_mgr_->getPivotSubtreeBeforeTimeStamp(hash.toString(), stamp);
  return subtree;
}

std::vector<std::string> FullNode::getDagBlockTips(blk_hash_t const &hash,
                                                   time_stamp_t stamp) {
  std::vector<std::string> tips =
      dag_mgr_->getTotalLeavesBeforeTimeStamp(hash.toString(), stamp);
  return tips;
}

std::vector<std::string> FullNode::getDagBlockPivotChain(blk_hash_t const &hash,
                                                         time_stamp_t stamp) {
  std::vector<std::string> pivot_chain =
      dag_mgr_->getPivotChainBeforeTimeStamp(hash.toString(), stamp);
  return pivot_chain;
}

std::vector<std::string> FullNode::getDagBlockEpFriend(blk_hash_t const &from,
                                                       blk_hash_t const &to) {
  std::vector<std::string> epfriend =
      dag_mgr_->getEpFriendBetweenPivots(from.toString(), to.toString());
  return epfriend;
}

std::vector<std::string> FullNode::getDagBlockSiblings(blk_hash_t const &hash,
                                                       time_stamp_t stamp) {
  auto blk = getDagBlock(hash);
  std::vector<std::string> parents;
  parents.emplace_back(blk->getPivot().toString());

  std::vector<std::string> siblings;
  for (auto const &parent : parents) {
    std::vector<std::string> children;
    children = dag_mgr_->getPivotChildrenBeforeTimeStamp(parent, stamp);
    siblings.insert(siblings.end(), children.begin(), children.end());
  }
  return siblings;
}

// return {period, block order}, for pbft-pivot-blk proposing
std::pair<uint64_t, std::shared_ptr<vec_blk_t>>
FullNode::createPeriodAndComputeBlockOrder(blk_hash_t const &anchor) {
  vec_blk_t orders;
  auto period = dag_mgr_->createPeriodAndComputeBlockOrder(anchor, orders);
  return {period, std::make_shared<vec_blk_t>(orders)};
}
// receive pbft-povit-blk, update periods
void FullNode::updateBlkDagPeriods(blk_hash_t const &anchor, uint64_t period) {
  dag_mgr_->setDagBlockPeriods(anchor, period);
}

std::shared_ptr<TrxSchedule> FullNode::createMockTrxSchedule(
    std::shared_ptr<vec_blk_t> blk_order) {
  if (!blk_order) {
    LOG(log_er_) << "Blk order NULL, cannot create mock trx schedule ...";
    return nullptr;
  }
  if (blk_order->empty()) {
    LOG(log_wr_)
        << "Blk order is empty ..., create empty mock trx schedule ...";
  }

  std::vector<std::vector<uint>> modes;
  for (auto const &b : *blk_order) {
    auto blk = getDagBlock(b);
    if (!blk) {
      LOG(log_er_) << "Cannot create schedule blk, blk missing ... " << b
                   << std::endl;
      return nullptr;
    }
    auto num_trx = blk->getTrxs().size();
    modes.emplace_back(std::vector<uint>(num_trx, 1));
  }
  return std::make_shared<TrxSchedule>(*blk_order, modes);
}

uint64_t FullNode::getNumReceivedBlocks() { return received_blocks_; }

uint64_t FullNode::getNumProposedBlocks() {
  return BlockProposer::getNumProposedBlocks();
}

std::pair<uint64_t, uint64_t> FullNode::getNumVerticesInDag() {
  return dag_mgr_->getNumVerticesInDag();
}

std::pair<uint64_t, uint64_t> FullNode::getNumEdgesInDag() {
  return dag_mgr_->getNumEdgesInDag();
}

void FullNode::drawGraph(std::string const &dotfile) const {
  dag_mgr_->drawPivotGraph("pivot." + dotfile);
  dag_mgr_->drawTotalGraph("total." + dotfile);
}

std::unordered_map<trx_hash_t, Transaction> FullNode::getNewVerifiedTrxSnapShot(
    bool onlyNew) {
  return trx_mgr_->getNewVerifiedTrxSnapShot(onlyNew);
}

void FullNode::insertBroadcastedTransactions(
    // transactions coming from broadcastin is less critical
    std::unordered_map<trx_hash_t, Transaction> const &transactions) {
  for (auto const &trx : transactions) {
    trx_mgr_->insertTrx(trx.second, false /* critical */);
    LOG(log_time_dg_) << "Transaction " << trx.second.getHash()
                      << " brkreceived at: " << getCurrentTimeMilliSeconds();
  }
}

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }

std::pair<bal_t, bool> FullNode::getBalance(addr_t const &acc) const {
  std::string bal = db_accs_->get(acc.toString());
  bool ret = false;
  if (bal.empty()) {
    LOG(log_tr_) << "Account " << acc << " not exist ..." << std::endl;
    bal = "0";
  } else {
    LOG(log_tr_) << "Account " << acc << "balance: " << bal << std::endl;
    ret = true;
  }
  return {std::stoull(bal), ret};
}

bool FullNode::setBalance(addr_t const &acc, bal_t const &new_bal) {
  bool ret = true;
  if (!db_accs_->update(acc.toString(), std::to_string(new_bal))) {
    LOG(log_wr_) << "Account " << acc.toString() << " update fail ..."
                 << std::endl;
  }
  return ret;
}

addr_t FullNode::getAddress() const { return node_addr_; }

dev::Signature FullNode::signMessage(std::string message) {
  return dev::sign(node_sk_, dev::sha3(message));
}

bool FullNode::verifySignature(dev::Signature const &signature,
                               std::string &message) {
  return dev::verify(node_pk_, signature, dev::sha3(message));
}
bool FullNode::executeScheduleBlock(ScheduleBlock const &sche_blk) {
  executor_->execute(sche_blk.getSchedule());
  return true;
}

void FullNode::placeVote(taraxa::blk_hash_t const &blockhash,
                         PbftVoteTypes type, uint64_t period, size_t step) {
  vote_queue_->placeVote(node_pk_, node_sk_, blockhash, type, period, step);
}

std::vector<Vote> FullNode::getVotes(uint64_t period) {
  return vote_queue_->getVotes(period);
}

void FullNode::placeVote(taraxa::Vote const &vote) {
  addr_t vote_address = dev::toAddress(vote.getPublicKey());
  std::pair<bal_t, bool> account_balance = getBalance(vote_address);
  if (!account_balance.second) {
    LOG(log_er_) << "Cannot find the vote account balance: " << vote_address
                 << std::endl;
    return;
  }
  if (vote.validateVote(account_balance)) {
    vote_queue_->placeVote(vote);
  }
}

void FullNode::broadcastVote(taraxa::blk_hash_t const &blockhash,
                             PbftVoteTypes type, uint64_t period, size_t step) {
  std::string message = blockhash.toString() + std::to_string(type) +
                        std::to_string(period) + std::to_string(step);
  dev::Signature signature = signMessage(message);
  Vote vote(node_pk_, signature, blockhash, type, period, step);
  network_->onNewPbftVote(vote);
}

bool FullNode::shouldSpeak(blk_hash_t const &blockhash, PbftVoteTypes type,
                           uint64_t period, size_t step) {
  return pbft_mgr_->shouldSpeak(blockhash, type, period, step);
}

void FullNode::clearVoteQueue() { vote_queue_->clearQueue(); }

size_t FullNode::getVoteQueueSize() { return vote_queue_->getSize(); }

bool FullNode::isKnownVote(taraxa::Vote const &vote) const {
  return known_votes_.count(vote.getHash());
}

void FullNode::setVoteKnown(taraxa::Vote const &vote) {
  known_votes_.insert(vote.getHash());
}

bool FullNode::isKnownPbftBlockInChain(
    taraxa::blk_hash_t const &pbft_block_hash) const {
  return pbft_chain_->findPbftBlockInChain(pbft_block_hash);
}

bool FullNode::isKnownPbftBlockInQueue(
    taraxa::blk_hash_t const &pbft_block_hash) const {
  return pbft_chain_->findPbftBlockInQueue(pbft_block_hash);
}

void FullNode::pushPbftBlockIntoQueue(taraxa::PbftBlock const &pbft_block) {
  pbft_chain_->pushPbftBlockIntoQueue(pbft_block);
}

size_t FullNode::getPbftChainSize() const {
  return pbft_chain_->getPbftChainSize();
}

size_t FullNode::getPbftQueueSize() const {
  return pbft_chain_->getPbftQueueSize();
}

size_t FullNode::getEpoch() const { return dag_mgr_->getEpoch(); }

bool FullNode::setPbftBlock(taraxa::PbftBlock const &pbft_block) {
  if (pbft_block.getBlockType() == pivot_block_type) {
    if (!pbft_chain_->pushPbftPivotBlock(pbft_block)) {
      return false;
    }
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    if (!pbft_chain_->pushPbftScheduleBlock(pbft_block)) {
      return false;
    }
  }
  // TODO: push other type pbft block into pbft chain

  // store pbft block into DB
  if (!db_pbftchain_->put(pbft_block.getBlockHash().toString(),
                          pbft_block.getJsonStr())) {
    LOG(log_er_) << "Failed put pbft block: " << pbft_block.getBlockHash()
                 << " into DB";
    return false;
  }
  if (!db_pbftchain_->update(pbft_chain_->getGenesisHash().toString(),
                             pbft_chain_->getJsonStr())) {
    LOG(log_er_) << "Failed update pbft genesis in DB";
    return false;
  }
  db_pbftchain_->commit();

  return true;
}

}  // namespace taraxa
