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
#include <boost/filesystem.hpp>
#include <chrono>
#include "SimpleDBFactory.h"
#include "block_proposer.hpp"
#include "dag.hpp"
#include "dag_block.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "sortition.h"
#include "state_registry.hpp"
#include "util_eth.hpp"
#include "vote.h"

namespace taraxa {

using std::string;
using std::to_string;
using util::eth::newDB;

void FullNode::setVerbose(bool verbose) {
  verbose_ = verbose;
  dag_mgr_->setVerbose(verbose);
  // TODO: setup logs for DB
  // db_blks_->setVerbose(verbose);
}

void FullNode::setDebug(bool debug) { debug_ = debug; }

FullNode::FullNode(boost::asio::io_context &io_context,
                   std::string const &conf_full_node_file, bool destroy_db,
                   bool rebuild_network)
    : FullNode(io_context, FullNodeConfig(conf_full_node_file), destroy_db,
               rebuild_network) {}
FullNode::FullNode(boost::asio::io_context &io_context,
                   FullNodeConfig const &conf_full_node, bool destroy_db,
                   bool rebuild_network) try
    : io_context_(io_context),
      num_block_workers_(conf_full_node.dag_processing_threads),
      conf_(conf_full_node),
      dag_mgr_(std::make_shared<DagManager>(
          conf_.genesis_state.block.getHash().toString())),
      blk_mgr_(std::make_shared<BlockManager>(1024 /*capacity*/,
                                              4 /* verifer thread*/)),
      trx_mgr_(std::make_shared<TransactionManager>()),
      trx_order_mgr_(std::make_shared<TransactionOrderManager>()),

      blk_proposer_(std::make_shared<BlockProposer>(
          conf_.test_params.block_proposer, dag_mgr_->getShared(),
          trx_mgr_->getShared())),
      vote_mgr_(std::make_shared<VoteManager>()),
      pbft_mgr_(std::make_shared<PbftManager>(
          conf_.test_params.pbft,
          conf_.genesis_state.block.getHash().toString())),
      pbft_chain_(std::make_shared<PbftChain>()) {
  LOG(log_nf_) << "Read FullNode Config: " << std::endl << conf_ << std::endl;

  auto key = dev::KeyPair::create();
  if (conf_.node_secret.empty()) {
    LOG(log_si_) << "New key generated " << toHex(key.secret().ref());
  } else {
    auto secret = dev::Secret(conf_.node_secret,
                              dev::Secret::ConstructFromStringType::FromHex);
    key = dev::KeyPair(secret);
  }
  if (rebuild_network) {
    network_ = std::make_shared<Network>(
        conf_full_node.network, "", key.secret(),
        conf_.genesis_state.block.getHash().toString());
  } else {
    network_ = std::make_shared<Network>(
        conf_full_node.network, conf_.db_path + "/net", key.secret(),
        conf_.genesis_state.block.getHash().toString());
  }
  node_sk_ = key.secret();
  node_pk_ = key.pub();
  node_addr_ = key.address();

  LOG(log_si_) << "Node public key: " << EthGreen << node_pk_.toString()
               << std::endl;
  LOG(log_si_) << "Node address: " << EthRed << node_addr_.toString()
               << std::endl;
  LOG(log_si_) << "Number of block works: " << num_block_workers_;
  initDB(destroy_db);

  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;

} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  throw e;
}
void FullNode::initDB(bool destroy_db) {
  if (!stopped_) {
    LOG(log_er_) << "Cannot init DB if node started ...";
    return;
  }
  if (db_blks_ == nullptr) {
    db_blks_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.block_db_path(), destroy_db);
    assert(db_blks_);
  }
  if (db_blks_index_ == nullptr) {
    db_blks_index_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.block_index_db_path(), destroy_db);
    assert(db_blks_index_);
  }
  if (db_trxs_ == nullptr) {
    db_trxs_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.transactions_db_path(), destroy_db);
    assert(db_trxs_);
  }
  if (db_trxs_to_blk_ == nullptr) {
    db_trxs_to_blk_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.trxs_to_blk_db_path(), destroy_db);
    assert(db_trxs_to_blk_);
  }
  if (db_votes_ == nullptr) {
    db_votes_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.pbft_votes_db_path(), destroy_db);
    assert(db_votes_);
  }
  if (db_pbftchain_ == nullptr) {
    db_pbftchain_ = SimpleDBFactory::createDelegate<SimpleOverlayDBDelegate>(
        conf_.pbft_chain_db_path(), destroy_db);
    assert(db_pbftchain_);
  }
  if (state_registry_ == nullptr) {
    // THIS IS THE GENESIS
    // TODO extract to a function
    auto const &genesis_block = conf_.genesis_state.block;
    auto const &genesis_hash = genesis_block.getHash();
    // store genesis blk to db
    db_blks_->put(genesis_hash.toString(), genesis_block.getJsonStr());
    db_blks_->commit();
    // store pbft chain genesis(HEAD) block to db
    db_pbftchain_->put(pbft_chain_->getGenesisHash().toString(),
                       pbft_chain_->getJsonStr());
    db_pbftchain_->commit();
    // TODO add move to a StateRegistry constructor?
    auto mode = destroy_db ? dev::WithExisting::Kill : dev::WithExisting::Trust;
    auto acc_db = newDB(conf_.account_db_path(),
                        genesis_hash,  //
                        mode);
    auto snapshot_db = newDB(conf_.account_snapshot_db_path(),
                             genesis_hash,  //
                             mode);
    state_registry_ =
        make_shared<StateRegistry>(conf_.genesis_state,
                                   dev::OverlayDB(move(acc_db.db)),  //
                                   move(snapshot_db.db));
    state_ =
        make_shared<StateRegistry::State>(state_registry_->getCurrentState());
  }
  LOG(log_nf_) << "DB initialized ...";
  db_inited_ = true;
  bool boot_node_balance_initialized = false;

  // init master boot node ...
  for (auto &node : conf_.network.network_boot_nodes) {
    auto node_public_key = dev::Public(node.id);

    // Assume only first boot node is initialized
    if (boot_node_balance_initialized == false) {
      addr_t master_boot_node_address(dev::toAddress(node_public_key));
      boot_node_balance_initialized = true;
      setMasterBootNodeAddress(master_boot_node_address);
    }
  }

  // Reconstruct DAG
  if (!destroy_db) {
    unsigned long level = 1;
    while (true) {
      h256 level_key(level);
      string entry = db_blks_index_->get(level_key.toString());
      if (entry.empty()) break;
      vector<string> blocks;
      boost::split(blocks, entry, boost::is_any_of(","));
      for (auto const &block : blocks) {
        auto block_json = db_blks_->get(block);
        if (block_json != "") {
          auto blk = DagBlock(block_json);
          dag_mgr_->addDagBlock(blk);
          max_dag_level_ = level;
        }
      }
      level++;
    }
  }

  LOG(log_wr_) << "DB initialized ... ";
}
// must call close() before destroyDB
bool FullNode::destroyDB() {
  if (!stopped_) {
    LOG(log_wr_) << "Cannot destroyDb if node is running ...";
    return false;
  }
  // make sure all sub moduled has relased DB, the full_node can release the
  // DB and destroy
  assert(db_blks_.use_count() == 1);
  assert(db_blks_index_.use_count() == 1);
  assert(db_trxs_.use_count() == 1);
  assert(db_trxs_to_blk_.use_count() == 1);
  assert(db_votes_.use_count() == 1);
  assert(db_pbftchain_.use_count() == 1);
  assert(state_registry_.use_count() == 1);
  assert(state_.use_count() == 1);

  db_blks_ = nullptr;
  db_blks_index_ = nullptr;
  db_trxs_ = nullptr;
  db_trxs_to_blk_ = nullptr;
  db_votes_ = nullptr;
  db_pbftchain_ = nullptr;
  state_registry_ = nullptr;
  state_ = nullptr;
  db_inited_ = false;
  thisThreadSleepForMilliSeconds(1000);
  boost::filesystem::path path(conf_.db_path);
  if (path.size() == 0) {
    throw std::invalid_argument("Error, invalid db path: " + conf_.db_path);
    return false;
  }
  if (boost::filesystem::exists(path)) {
    LOG(log_wr_) << "Delete db directory: " << path;
    if (!boost::filesystem::remove_all(path)) {
      throw std::invalid_argument("Error, cannot delete db path: " +
                                  conf_.db_path);
      return false;
    }
  }
  LOG(log_wr_) << "DB destroyed ... ";

  return true;
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
  if (!db_inited_) {
    initDB(false);
  }
  stopped_ = false;
  // order depend, be careful when changing the order
  network_->setFullNode(getShared());
  network_->start(boot_node);
  dag_mgr_->start();
  blk_mgr_->setFullNode(getShared());
  blk_mgr_->start();
  trx_mgr_->setFullNode(getShared());
  trx_mgr_->start();
  trx_order_mgr_->setFullNode(getShared());
  trx_order_mgr_->start();
  blk_proposer_->setFullNode(getShared());
  blk_proposer_->start();
  vote_mgr_->setFullNode(getShared());
  pbft_mgr_->setFullNode(getShared());
  pbft_mgr_->start();
  executor_ =
      std::make_shared<Executor>(pbft_mgr_->VALID_SORTITION_COINS, log_time_,
                                 db_blks_, db_trxs_, state_registry_);
  executor_->setFullNode(getShared());
  i_am_boot_node_ = boot_node;
  if (i_am_boot_node_) {
    LOG(log_nf_) << "Starting a boot node ..." << std::endl;
  }
  block_workers_.clear();
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
          auto level = blk.getLevel();
          h256 level_key(level);
          std::string blocks = db_blks_index_->get(level_key.toString());
          if (level > max_dag_level_) max_dag_level_ = level;
          if (blocks == "") {
            db_blks_index_->put(level_key.toString(), blk.getHash().toString());
          } else {
            auto newblocks = blocks + "," + blk.getHash().toString();
            db_blks_index_->update(level_key.toString(),
                                   blocks + "," + blk.getHash().hex());
          }
          db_blks_index_->commit();
        }
        network_->onNewBlockVerified(blk);
        LOG(log_time_) << "Broadcast block " << blk.getHash()
                       << " at: " << getCurrentTimeMilliSeconds();
      }
    });
  }
  assert(num_block_workers_ == block_workers_.size());
  assert(db_blks_);
  assert(db_blks_index_);
  assert(db_trxs_);
  assert(db_trxs_to_blk_);
  assert(db_votes_);
  assert(db_pbftchain_);
  assert(state_registry_);
  assert(state_);
  LOG(log_wr_) << "Node started ... ";
}

void FullNode::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;

  dag_mgr_->stop();  // dag_mgr_ stopped, notify blk_proposer ...
  blk_proposer_->stop();
  blk_mgr_->stop();
  // Do not stop network_, o.w. restart node will crash
  // network_->stop();
  trx_mgr_->stop();
  trx_order_mgr_->stop();
  pbft_mgr_->stop();
  executor_ = nullptr;

  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_[i].join();
  }
  // wait a while to let other modules to stop
  thisThreadSleepForMilliSeconds(100);
  assert(db_blks_.use_count() == 1);
  assert(db_blks_index_.use_count() == 1);
  assert(db_trxs_.use_count() == 1);
  assert(db_trxs_to_blk_.use_count() == 1);

  assert(db_votes_.use_count() == 1);
  assert(db_pbftchain_.use_count() == 1);
  assert(state_registry_.use_count() == 1);
  assert(state_.use_count() == 1);
  LOG(log_wr_) << "Node stopped ... ";
}

bool FullNode::reset() {
  destroyDB();
  network_ = nullptr;
  dag_mgr_ = nullptr;
  blk_mgr_ = nullptr;
  trx_mgr_ = nullptr;
  trx_order_mgr_ = nullptr;
  blk_proposer_ = nullptr;
  executor_ = nullptr;
  vote_mgr_ = nullptr;
  pbft_mgr_ = nullptr;
  pbft_chain_ = nullptr;

  assert(network_.use_count() == 0);
  // dag
  assert(dag_mgr_.use_count() == 0);
  // ledger
  assert(blk_mgr_.use_count() == 0);
  assert(trx_mgr_.use_count() == 0);
  assert(trx_order_mgr_.use_count() == 0);
  // block proposer (multi processing)
  assert(blk_proposer_.use_count() == 0);
  // transaction executor
  assert(executor_.use_count() == 0);
  // PBFT
  assert(vote_mgr_.use_count() == 0);

  assert(pbft_mgr_.use_count() == 0);

  assert(pbft_chain_.use_count() == 0);

  max_dag_level_ = 0;
  received_blocks_ = 0;
  received_trxs_ = 0;

  // init
  network_ =
      std::make_shared<Network>(conf_.network, "", node_sk_,
                                conf_.genesis_state.block.getHash().toString());
  blk_mgr_ =
      std::make_shared<BlockManager>(1024 /*capacity*/, 4 /* verifer thread*/);
  trx_mgr_ = std::make_shared<TransactionManager>();
  trx_order_mgr_ = std::make_shared<TransactionOrderManager>();
  dag_mgr_ = std::make_shared<DagManager>(
      conf_.genesis_state.block.getHash().toString());
  blk_proposer_ = std::make_shared<BlockProposer>(
      conf_.test_params.block_proposer, dag_mgr_->getShared(),
      trx_mgr_->getShared());
  pbft_mgr_ = std::make_shared<PbftManager>(
      conf_.test_params.pbft, conf_.genesis_state.block.getHash().toString());
  vote_mgr_ = std::make_shared<VoteManager>();
  pbft_chain_ = std::make_shared<PbftChain>();
  executor_ =
      std::make_shared<Executor>(pbft_mgr_->VALID_SORTITION_COINS, log_time_,
                                 db_blks_, db_trxs_, state_registry_);
  LOG(log_wr_) << "Node reset ... ";
  return true;
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

std::vector<std::shared_ptr<DagBlock>> FullNode::getDagBlocksAtLevel(
    unsigned long level, int number_of_levels) {
  std::vector<std::shared_ptr<DagBlock>> res;
  for (int i = 0; i < number_of_levels; i++) {
    if (level + i == 0) continue;  // Skip genesis
    dev::h256 level_key(level + i);
    string entry = db_blks_index_->get(level_key.toString());

    if (entry.empty()) break;
    vector<string> blocks;
    boost::split(blocks, entry, boost::is_any_of(","));
    for (auto const &block : blocks) {
      auto block_json = db_blks_->get(block);
      if (block_json != "") {
        res.push_back(std::make_shared<DagBlock>(block_json));
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
std::pair<uint64_t, std::shared_ptr<vec_blk_t>> FullNode::getDagBlockOrder(
    blk_hash_t const &anchor) {
  vec_blk_t orders;
  auto period = dag_mgr_->getDagBlockOrder(anchor, orders);
  return {period, std::make_shared<vec_blk_t>(orders)};
}
// receive pbft-povit-blk, update periods
uint FullNode::setDagBlockOrder(blk_hash_t const &anchor, uint64_t period) {
  return dag_mgr_->setDagBlockPeriod(anchor, period);
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
    LOG(log_err_) << "Transaction overlap table nullptr, cannot create mock "
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

std::pair<val_t, bool> FullNode::getBalance(addr_t const &acc) const {
  auto const &state = updateAndGetState();
  auto bal = state->balance(acc);
  if (bal == 0 && !state->addressInUse(acc)) {
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
bool FullNode::executeScheduleBlock(
    ScheduleBlock const &sche_blk,
    std::unordered_map<addr_t, val_t> &sortition_account_balance_table) {
  return executor_->execute(sche_blk.getSchedule(),
                            sortition_account_balance_table);
}

std::vector<Vote> FullNode::getVotes(uint64_t round) {
  return vote_mgr_->getVotes(round);
}

void FullNode::addVote(taraxa::Vote const &vote) { vote_mgr_->addVote(vote); }

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

bool FullNode::isKnownVote(uint64_t pbft_round,
                           vote_hash_t const &vote_hash) const {
  return vote_mgr_->isKnownVote(pbft_round, vote_hash);
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

void FullNode::newOrderedBlock(blk_hash_t const &dag_block_hash,
                               uint64_t const &block_number) {
  auto blk = getDagBlock(dag_block_hash);
  if (ws_server_) ws_server_->newOrderedBlock(blk, block_number);
}

bool FullNode::setPbftBlock(taraxa::PbftBlock const &pbft_block) {
  if (pbft_block.getBlockType() == pivot_block_type) {
    if (!pbft_chain_->pushPbftPivotBlock(pbft_block)) {
      return false;
    }
    // get dag blocks order
    blk_hash_t dag_block_hash = pbft_block.getPivotBlock().getDagBlockHash();
    uint64_t current_period;
    std::shared_ptr<vec_blk_t> dag_blocks_order;
    std::tie(current_period, dag_blocks_order) =
        getDagBlockOrder(dag_block_hash);
    // update DAG blocks order and DAG blocks table
    for (auto const &dag_blk_hash : *dag_blocks_order) {
      auto block_number = pbft_chain_->pushDagBlockHash(dag_blk_hash);
      newOrderedBlock(dag_blk_hash, block_number);
    }
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    if (!pbft_chain_->pushPbftScheduleBlock(pbft_block)) {
      return false;
    }
    // set Dag blocks period
    blk_hash_t last_pivot_block_hash =
        pbft_block.getScheduleBlock().getPrevBlockHash();
    std::pair<PbftBlock, bool> last_pivot_block =
        pbft_chain_->getPbftBlockInChain(last_pivot_block_hash);
    if (!last_pivot_block.second) {
      LOG(log_err_) << "Cannot find the last pivot block hash "
                    << last_pivot_block_hash
                    << " in pbft chain. Should never happen here!";
      assert(false);
    }
    blk_hash_t dag_block_hash =
        last_pivot_block.first.getPivotBlock().getDagBlockHash();
    uint64_t current_pbft_chain_period =
        last_pivot_block.first.getPivotBlock().getPeriod();
    uint dag_ordered_blocks_size =
        setDagBlockOrder(dag_block_hash, current_pbft_chain_period);
    // checking: DAG ordered blocks size in this period should equal to the
    // DAG blocks inside PBFT CS block
    uint dag_blocks_inside_pbft_cs =
        pbft_block.getScheduleBlock().getSchedule().blk_order.size();
    if (dag_ordered_blocks_size != dag_blocks_inside_pbft_cs) {
      LOG(log_err_) << "Setting DAG block order finalize "
                    << dag_ordered_blocks_size << " blocks."
                    << " But the PBFT CS block has "
                    << dag_blocks_inside_pbft_cs << " DAG block hash.";
      // TODO: need to handle the error condition(should never happen)
    }

    // TODO: VM executor will not take sortition_account_balance_table as
    // reference.
    //  But will return a list of modified accounts as pairs<addr_t, val_t>.
    //  Will need update sortition_account_balance_table here
    // execute schedule block
    if (!executeScheduleBlock(pbft_block.getScheduleBlock(),
                              pbft_mgr_->sortition_account_balance_table)) {
      LOG(log_er_) << "Failed to execute schedule block";
      // TODO: If valid transaction failed execute, how to do?
    }
    // reset sortition_threshold and TWO_T_PLUS_ONE
    size_t accounts = pbft_mgr_->sortition_account_balance_table.size();
    size_t two_t_plus_one;
    size_t sortition_threshold;
    if (pbft_mgr_->COMMITTEE_SIZE <= accounts) {
      two_t_plus_one = pbft_mgr_->COMMITTEE_SIZE * 2 / 3 + 1;
      sortition_threshold = pbft_mgr_->COMMITTEE_SIZE;
    } else {
      two_t_plus_one = accounts * 2 / 3 + 1;
      sortition_threshold = accounts;
    }
    pbft_mgr_->setTwoTPlusOne(two_t_plus_one);
    pbft_mgr_->setSortitionThreshold(sortition_threshold);
    LOG(log_deb_) << "Update 2t+1 " << two_t_plus_one << " Threshold "
                  << sortition_threshold;
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

Vote FullNode::generateVote(blk_hash_t const &blockhash, PbftVoteTypes type,
                            uint64_t period, size_t step) {
  blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  // sortition signature
  sig_t sortition_signature =
      vote_mgr_->signVote(node_sk_, last_pbft_block_hash, type, period, step);
  // vote signature
  sig_t vote_signature =
      vote_mgr_->signVote(node_sk_, blockhash, type, period, step);

  Vote vote(node_pk_, sortition_signature, vote_signature, blockhash, type,
            period, step);
  LOG(log_deb_) << "last pbft block hash " << last_pbft_block_hash
                << " vote: " << vote.getHash();

  return vote;
}

level_t FullNode::getMaxDagLevel() const { return dag_mgr_->getMaxLevel(); }

std::pair<blk_hash_t, bool> FullNode::getDagBlockHash(
    uint64_t dag_block_height) const {
  return pbft_chain_->getDagBlockHash(dag_block_height);
}

std::pair<uint64_t, bool> FullNode::getDagBlockHeight(
    blk_hash_t const &dag_block_hash) const {
  return pbft_chain_->getDagBlockHeight(dag_block_hash);
}

uint64_t FullNode::getDagBlockMaxHeight() const {
  return pbft_chain_->getDagBlockMaxHeight();
}

std::vector<blk_hash_t> FullNode::getLinearizedDagBlocks() const {
  std::vector<blk_hash_t> ret;
  auto max_height = getDagBlockMaxHeight();
  for (auto i(0); i <= max_height; ++i) {
    auto blk = getDagBlockHash(i);
    assert(blk.second);
    ret.emplace_back(blk.first);
  }
  return ret;
}

}  // namespace taraxa
