/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-01 15:43:56
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 20:01:50
 */

#include "full_node.hpp"
#include <boost/asio.hpp>
#include "block_proposer.hpp"
#include "dag.hpp"
#include "dag_block.hpp"
#include "executor.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "rocks_db.hpp"
#include "transaction.hpp"
#include "vote.h"

namespace taraxa {

using std::string;
using std::to_string;

FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    address =
        boost::asio::ip::address::from_string(doc.get<std::string>("address"));
    node_secret = doc.get<std::string>("node_secret");
    db_accounts_path = doc.get<std::string>("db_accounts_path");
    db_blocks_path = doc.get<std::string>("db_blocks_path");
    db_transactions_path = doc.get<std::string>("db_transactions_path");
    dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");

    proposer.mode = doc.get<uint>("block_proposer.mode");
    proposer.param1 = doc.get<uint>("block_proposer.param1");
    proposer.param2 = doc.get<uint>("block_proposer.param2");

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void FullNode::setVerbose(bool verbose) {
  verbose_ = verbose;
  dag_mgr_->setVerbose(verbose);
  db_blks_->setVerbose(verbose);
}

void FullNode::setDebug(bool debug) { debug_ = debug; }

FullNode::FullNode(boost::asio::io_context &io_context,
                   std::string const &conf_full_node,
                   std::string const &conf_network)
    : FullNode(io_context, FullNodeConfig(conf_full_node),
               NetworkConfig(conf_network)) {}

FullNode::FullNode(boost::asio::io_context &io_context,
                   FullNodeConfig const &conf_full_node,
                   NetworkConfig const &conf_network) try
    : io_context_(io_context),
      conf_(conf_full_node),
      db_accs_(std::make_shared<RocksDb>(conf_.db_accounts_path)),
      db_blks_(std::make_shared<RocksDb>(conf_.db_blocks_path)),
      db_trxs_(std::make_shared<RocksDb>(conf_.db_transactions_path)),
      blk_qu_(std::make_shared<BlockQueue>(1024 /*capacity*/,
                                           2 /* verifer thread*/)),
      trx_mgr_(std::make_shared<TransactionManager>(db_blks_, db_trxs_)),
      network_(std::make_shared<Network>(conf_network)),
      dag_mgr_(std::make_shared<DagManager>(conf_.dag_processing_threads)),
      blk_proposer_(std::make_shared<BlockProposer>(conf_.proposer,
                                                    dag_mgr_->getShared(),
                                                    trx_mgr_->getShared())),
      executor_(std::make_shared<Executor>(db_blks_->getShared(),
                                           db_trxs_->getShared(),
                                           db_accs_->getShared())),
      pbft_mgr_(std::make_shared<PbftManager>()),
      vote_queue_(std::make_shared<VoteQueue>()) {
  LOG(log_si_) << "Taraxa node statred at address: " << conf_.address << " ..."
               << std::endl;
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
  LOG(log_si_) << "Node public key: " << EthGreen << node_pk_.toString()
               << std::endl;
  LOG(log_si_) << "Node address: " << EthRed << node_addr_.toString()
               << std::endl;

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

void FullNode::start() {
  if (!stopped_) {
    return;
  }
  stopped_ = false;
  network_->setFullNode(getShared());
  network_->start();
  dag_mgr_->start();
  blk_qu_->start();
  blk_proposer_->setFullNode(getShared());
  blk_proposer_->start();
  trx_mgr_->start();
  pbft_mgr_->setFullNode(getShared());
  pbft_mgr_->start();
  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_.emplace_back([this]() {
      while (!stopped_) {
        auto blk = blk_qu_->getVerifiedBlock();
        if (stopped_) break;
        // Any transactions that are passed with the block were not verified in
        // transactions queue so they need to be verified here
        bool invalidTransaction = false;
        for (auto const &trx : blk.second) {
          auto valid = trx.verifySig();  // Probably move this check somewhere
                                         // in transaction classes later
          if (!valid) {
            invalidTransaction = true;
            LOG(log_er_) << "Invalid transaction " << trx.getHash().toString();
          }
        }
        // Skip block if invalid transaction
        if (invalidTransaction) continue;

        // Save the transaction that came with the block together with the
        // transactions that are in the queue This will update the transaction
        // status as well and remove the transactions from the queue
        trx_mgr_->saveBlockTransactionsAndUpdateTransactionStatus(
            blk.first.getTrxs(), blk.second);

        LOG(log_nf_) << "Write block to db ... " << blk.first.getHash()
                     << std::endl;
        if (debug_) {
          std::unique_lock<std::mutex> lock(debug_mutex_);
          if (!stopped_) {
            received_blocks_++;
          }
        }
        db_blks_->put(blk.first.getHash().toString(), blk.first.getJsonStr());
        dag_mgr_->addDagBlock(blk.first, true);
        network_->onNewBlockVerified(blk.first);
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
  blk_qu_->stop();
  network_->stop();
  trx_mgr_->stop();
  pbft_mgr_->stop();
  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_[i].join();
  }
}

size_t FullNode::getPeerCount() const { return network_->getPeerCount(); }
std::vector<public_t> FullNode::getAllPeers() const { return network_->getAllPeers(); }

void FullNode::storeBlockWithTransactions(
    DagBlock const &blk, std::vector<Transaction> const &transactions) {
  blk_qu_->pushUnverifiedBlock(std::move(blk), std::move(transactions));
}

void FullNode::storeBlock(DagBlock const &blk) {
  blk_qu_->pushUnverifiedBlock(std::move(blk));
}

void FullNode::storeBlockAndSign(DagBlock const &blk) {
  DagBlock sign_block(blk);
  sign_block.sign(node_sk_);
  LOG(log_nf_) << "Signed block: " << sign_block << std::endl;
  storeBlock(sign_block);
}

bool FullNode::isBlockKnown(blk_hash_t const &hash) {
  auto known = blk_qu_->isBlockKnown(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

void FullNode::storeTransaction(Transaction const &trx) {
  trx_mgr_->insertTrx(trx);
}

std::shared_ptr<DagBlock> FullNode::getDagBlock(blk_hash_t const &hash) {
  std::shared_ptr<DagBlock> blk;
  // find if in block queue
  blk = blk_qu_->getDagBlock(hash);
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
  std::string json = db_trxs_->get(hash.toString());
  if (!json.empty()) {
    return std::make_shared<Transaction>(json);
  }
  // Check the queue as well
  return trx_mgr_->getTransaction(hash);
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

std::vector<std::string> FullNode::getDagBlockEpochs(blk_hash_t const &from,
                                                     blk_hash_t const &to) {
  std::vector<std::string> epochs =
      dag_mgr_->getTotalEpochsBetweenBlocks(from.toString(), to.toString());
  return epochs;
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

void FullNode::insertNewTransactions(
    std::unordered_map<trx_hash_t, Transaction> const &transactions) {
  for (auto const &trx : transactions) trx_mgr_->insertTrx(trx.second);
}

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }

std::pair<bal_t, bool> FullNode::getBalance(addr_t const &acc) const {
  std::string bal = db_accs_->get(acc.toString());
  bool ret = false;
  if (bal.empty()) {
    LOG(log_wr_) << "Account " << acc << " not exist ..." << std::endl;
    bal = "0";
  } else {
    LOG(log_nf_) << "Account " << acc << "balance: " << bal << std::endl;
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

addr_t FullNode::getAddress() { return node_addr_; }

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

void FullNode::placeVote(taraxa::blk_hash_t const &blockhash, char type,
                         int period, int step) {
  vote_queue_->placeVote(node_pk_, node_sk_, blockhash, type, period, step);
}

std::vector<Vote> FullNode::getVotes(int period) {
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

void FullNode::broadcastVote(taraxa::blk_hash_t const &blockhash, char type,
                             int period, int step) {
  std::string message = blockhash.toString() + std::to_string(type) +
                        std::to_string(period) + std::to_string(step);
  dev::Signature signature = signMessage(message);
  Vote vote(node_pk_, signature, blockhash, type, period, step);
  network_->noNewPbftVote(vote);
}

bool FullNode::shouldSpeak(blk_hash_t const &blockhash, char type, int period,
                           int step) {
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

}  // namespace taraxa
