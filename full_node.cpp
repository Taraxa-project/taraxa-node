#include "full_node.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <chrono>

#include "block_proposer.hpp"
#include "dag.hpp"
#include "dag_block.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "sortition.h"
#include "util/eth.hpp"
#include "vote.h"

namespace taraxa {

using std::string;
using std::to_string;
using util::eth::newDB;
void FullNode::setDebug(bool debug) { debug_ = debug; }

FullNode::FullNode(std::string const &conf_full_node_file,
                   bool destroy_db,  //
                   bool rebuild_network)
    : FullNode(FullNodeConfig(conf_full_node_file),
               destroy_db,  //
               rebuild_network) {}

FullNode::FullNode(FullNodeConfig const &conf_full_node,
                   bool destroy_db,  //
                   bool rebuild_network)
    : num_block_workers_(conf_full_node.dag_processing_threads),
      conf_(conf_full_node),
      dag_mgr_(std::make_shared<DagManager>(
          conf_.dag_genesis_block.getHash().toString())),
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
          conf_.dag_genesis_block.getHash().toString())),
      pbft_chain_(std::make_shared<PbftChain>(
          conf_.dag_genesis_block.getHash().toString())) {
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
    network_ =
        std::make_shared<Network>(conf_full_node.network, "", key.secret(),
                                  conf_.dag_genesis_block.getHash().toString());
  } else {
    network_ = std::make_shared<Network>(
        conf_full_node.network, conf_.db_path + "/net", key.secret(),
        conf_.dag_genesis_block.getHash().toString());
  }
  node_sk_ = key.secret();
  node_pk_ = key.pub();
  node_addr_ = key.address();
  vrf_sk_ = vrf_sk_t(conf_.vrf_secret);
  vrf_pk_ = vrf_wrapper::getVrfPublicKey(vrf_sk_);
  LOG(log_si_) << "Node public key: " << EthGreen << node_pk_.toString()
               << std::endl;
  LOG(log_si_) << "Node address: " << EthRed << node_addr_.toString()
               << std::endl;
  LOG(log_si_) << "Node VRF public key: " << EthGreen << vrf_pk_.toString()
               << std::endl;
  LOG(log_si_) << "Number of block works: " << num_block_workers_;
  auto const &genesis_block = conf_.dag_genesis_block;
  if (!genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  auto const &genesis_hash = genesis_block.getHash();
  if (destroy_db) {
    boost::filesystem::remove_all(conf_.db_path);
  }
  {
    using namespace dev::db;
    auto db_path = conf_.replay_protection_service_db_path();
    auto db_result = newDB(db_path, genesis_hash, dev::WithExisting::Trust,
                           DatabaseKind::RocksDB);
    db_replay_protection_service_.reset((RocksDB *)db_result.db.release());
  }
  db_pbft_sortition_accounts_ = std::move(
      newDB(conf_.pbft_sortition_accounts_db_path(), genesis_hash).db);
  db_blks_ = std::make_shared<DatabaseFaceCache>(
      newDB(conf_.block_db_path(), genesis_hash).db, 10000);
  db_blks_index_ =
      std::move(newDB(conf_.block_index_db_path(), genesis_hash).db);
  db_trxs_ = std::make_shared<DatabaseFaceCache>(
      newDB(conf_.transactions_db_path(), genesis_hash).db, 100000);
  db_trxs_to_blk_ =
      std::move(newDB(conf_.trxs_to_blk_db_path(), genesis_hash).db);
  db_pbftchain_ = std::move(newDB(conf_.pbft_chain_db_path(), genesis_hash).db);
  db_pbft_blocks_order_ =
      std::move(newDB(conf_.pbft_blocks_order_db_path(), genesis_hash).db);
  db_dag_blocks_order_ =
      std::move(newDB(conf_.dag_blocks_order_path(), genesis_hash).db);
  db_dag_blocks_height_ =
      std::move(newDB(conf_.dag_blocks_height_path(), genesis_hash).db);
  db_cert_votes_ = std::make_shared<DatabaseFaceCache>(
      newDB(conf_.pbft_cert_votes_db_path(), genesis_hash).db, 100000);
  db_dag_blocks_period_ =
      std::move(newDB(conf_.dag_blocks_period_path(), genesis_hash).db);
  db_period_schedule_block_ =
      std::move(newDB(conf_.period_schedule_block_path(), genesis_hash).db);
  db_status_ = std::move(newDB(conf_.status_path(), genesis_hash).db);
  // store genesis blk to db
  db_blks_->insert(genesis_hash, genesis_block.rlp(true));
  LOG(log_nf_) << "DB initialized ...";
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
      string entry = db_blks_index_->lookup(level_key.toString());
      if (entry.empty()) break;
      vector<string> blocks;
      boost::split(blocks, entry, boost::is_any_of(","));
      for (auto const &block : blocks) {
        auto block_bytes = db_blks_->lookup(blk_hash_t(block));
        if (block_bytes.size() > 0) {
          auto blk = DagBlock(block_bytes);
          dag_mgr_->addDagBlock(blk);
        }
      }
      level++;
    }
  }
  LOG(log_wr_) << "DB initialized ... ";
  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;
}

std::shared_ptr<FullNode> FullNode::getShared() { return shared_from_this(); }

void FullNode::start(bool boot_node) {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  replay_protection_service_ = std::make_shared<ReplayProtectionService>(
      conf_.replay_protection_service_range, db_replay_protection_service_);
  eth_service_ = as_shared(
      new EthService(getShared(), conf_.eth_chain_params, conf_.eth_db_path()));
  executor_ = as_shared(new Executor(pbft_mgr_->VALID_SORTITION_COINS,
                                     log_time_,  //
                                     db_blks_,
                                     db_trxs_,                    //
                                     replay_protection_service_,  //
                                     eth_service_,                //
                                     db_status_,                  //
                                     conf_.use_basic_executor));
  executor_->setFullNode(getShared());
  // order depend, be careful when changing the order
  // setFullNode pbft_chain need be before network, otherwise db_pbftchain will
  // be nullptr
  pbft_chain_->setFullNode(getShared());
  network_->setFullNode(getShared());
  network_->start(boot_node);
  dag_mgr_->setFullNode(getShared());
  blk_mgr_->setFullNode(getShared());
  blk_mgr_->start();
  trx_mgr_->setFullNode(getShared());
  trx_mgr_->start();
  trx_order_mgr_->setFullNode(getShared());
  trx_order_mgr_->start();
  blk_proposer_->setFullNode(getShared());
  blk_proposer_->start();
  vote_mgr_->setFullNode(getShared());
  pbft_mgr_->setFullNode(getShared(), replay_protection_service_);
  pbft_mgr_->start();
  i_am_boot_node_ = boot_node;
  if (i_am_boot_node_) {
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

        if (dag_mgr_->addDagBlock(blk)) {
          {
            auto block_bytes = blk.rlp(true);
            db_blks_->insert(blk.getHash(), block_bytes);
            auto level = blk.getLevel();
            h256 level_key(level);
            std::string blocks = db_blks_index_->lookup(level_key.toString());
            if (blocks == "") {
              db_blks_index_->insert(level_key.toString(),
                                     blk.getHash().toString());
            } else {
              auto newblocks = blocks + "," + blk.getHash().toString();
              db_blks_index_->insert(level_key.toString(),
                                     blocks + "," + blk.getHash().hex());
            }
            if (ws_server_) ws_server_->newDagBlock(blk);
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
  trx_order_mgr_->stop();
  pbft_mgr_->stop();
  pbft_chain_->releaseDB();
  for (auto &t : block_workers_) {
    t.join();
  }
  executor_ = nullptr;
  eth_service_ = nullptr;
  replay_protection_service_ = nullptr;
  assert(db_replay_protection_service_.use_count() == 1);
  assert(db_blks_.use_count() == 1);
  assert(db_blks_index_.use_count() == 1);
  assert(db_trxs_.use_count() == 1);
  assert(db_trxs_to_blk_.use_count() == 1);
  assert(db_cert_votes_.use_count() == 1);
  assert(db_pbftchain_.use_count() == 1);
  assert(db_pbft_blocks_order_.use_count() == 1);
  assert(db_dag_blocks_order_.use_count() == 1);
  assert(db_dag_blocks_height_.use_count() == 1);
  assert(db_pbft_sortition_accounts_.use_count() == 1);
  assert(db_dag_blocks_period_.use_count() == 1);
  assert(db_period_schedule_block_.use_count() == 1);
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

bool FullNode::insertTransaction(Transaction const &trx) {
  auto rlp = trx.rlp(true);
  if (conf_.network.network_transaction_interval == 0) {
    network_->onNewTransactions({rlp});
  }
  return trx_mgr_->insertTrx(trx, trx.rlp(true), true);
}

std::shared_ptr<DagBlock> FullNode::getDagBlockFromDb(
    blk_hash_t const &hash) const {
  auto blk_bytes = db_blks_->lookup(hash);
  if (blk_bytes.size() > 0) {
    return std::make_shared<DagBlock>(blk_bytes);
  }
  return nullptr;
}

std::shared_ptr<DagBlock> FullNode::getDagBlock(blk_hash_t const &hash) const {
  std::shared_ptr<DagBlock> blk;
  // find if in block queue
  blk = blk_mgr_->getDagBlock(hash);
  // not in queue, search db
  if (!blk) {
    auto blk_bytes = db_blks_->lookup(hash);
    if (blk_bytes.size() > 0) {
      blk = std::make_shared<DagBlock>(blk_bytes);
    }
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

unsigned long FullNode::getTransactionStatusCount() const {
  if (stopped_ || !trx_mgr_) {
    return 0;
  }
  return trx_mgr_->getTransactionStatusCount();
}

unsigned long FullNode::getTransactionCount() const {
  if (stopped_ || !trx_mgr_) {
    return 0;
  }
  return trx_mgr_->getTransactionCount();
}

std::vector<std::shared_ptr<DagBlock>> FullNode::getDagBlocksAtLevel(
    unsigned long level, int number_of_levels) {
  std::vector<std::shared_ptr<DagBlock>> res;
  for (int i = 0; i < number_of_levels; i++) {
    if (level + i == 0) continue;  // Skip genesis
    dev::h256 level_key(level + i);
    string entry = db_blks_index_->lookup(level_key.toString());

    if (entry.empty()) break;
    vector<string> blocks;
    boost::split(blocks, entry, boost::is_any_of(","));
    for (auto const &block : blocks) {
      auto block_bytes = db_blks_->lookup(blk_hash_t(block));
      if (block_bytes.size() > 0) {
        res.push_back(std::make_shared<DagBlock>(block_bytes));
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
    trx_mgr_->insertTrx(trx, t, false /* critical */);
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
bool FullNode::executePeriod(PbftBlock const &pbft_block,
                             std::unordered_map<addr_t, PbftSortitionAccount>
                                 &sortition_account_balance_table,
                             uint64_t period) {
  auto const &sche_blk = pbft_block.getScheduleBlock();
  // TODO: Since PBFT use replay protection serivce and no longer include
  //  overlapped/invalid transations in schedule block. May not need transtion
  //  overlap table anymore. CCL please check here
  // update transaction overlap table first
  auto res = trx_order_mgr_->updateOrderedTrx(sche_blk.getSchedule());
  res |=
      executor_->execute(pbft_block, sortition_account_balance_table, period);
  uint64_t block_number = 0;
  if (sche_blk.getSchedule().dag_blks_order.size() > 0) {
    block_number =
        pbft_chain_->getDagBlockHeight(sche_blk.getSchedule().dag_blks_order[0])
            .first;
  }
  if (ws_server_) {
    ws_server_->newScheduleBlockExecuted(sche_blk, block_number, period);
  }
  return res;
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

uint64_t FullNode::getPbftChainSize() const {
  return pbft_chain_->getPbftChainSize();
}

void FullNode::newOrderedBlock(blk_hash_t const &dag_block_hash,
                               uint64_t const &block_number) {
  auto blk = getDagBlock(dag_block_hash);
  if (ws_server_) ws_server_->newOrderedBlock(blk, block_number);
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
  VrfSortition vrf_sortition(vrf_sk_, last_pbft_block_hash, type, period, step);
  Vote vote(node_sk_, vrf_sortition, blockhash);

  LOG(log_dg_) << "last pbft block hash " << last_pbft_block_hash
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
  for (auto i(1); i <= max_height; ++i) {
    auto blk = getDagBlockHash(i);
    assert(blk.second);
    ret.emplace_back(blk.first);
  }
  return ret;
}
void FullNode::updateNonceTable(DagBlock const &blk,
                                DagFrontier const &frontier) {
  trx_mgr_->updateNonce(blk, frontier);
}

std::vector<trx_hash_t> FullNode::getPackedTrxs() const {
  std::unordered_set<trx_hash_t> packed_trxs;
  auto max_height = getDagBlockMaxHeight();
  for (auto i(1); i <= max_height; ++i) {
    auto blk = getDagBlockHash(i);
    assert(blk.second);
    auto dag_blk = getDagBlock(blk.first);
    assert(dag_blk);
    for (auto const &t : dag_blk->getTrxs()) {
      packed_trxs.insert(t);
    }
  }
  std::vector<trx_hash_t> ret;
  for (auto const &t : packed_trxs) {
    ret.emplace_back(t);
  }
  return ret;
}

void FullNode::storeCertVotes(blk_hash_t const &pbft_hash,
                              std::vector<Vote> const &votes) {
  RLPStream s;
  s.appendList(votes.size());
  for (auto const &v : votes) {
    // LOG(log_sil_) << v;
    s.append(v.rlp());
  }
  auto ss = s.out();
  db_cert_votes_->insert(pbft_hash, ss);
  LOG(log_dg_) << "Storing cert votes of pbft blk " << pbft_hash << "\n"
               << votes;
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

TransactionUnsafeStatusTable FullNode::getUnsafeTransactionStatusTable() const {
  return trx_mgr_->getUnsafeTransactionStatusTable();
}

std::unordered_set<std::string> FullNode::getUnOrderedDagBlks() const {
  return dag_mgr_->getUnOrderedDagBlks();
}

}  // namespace taraxa