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
#include "network.hpp"
#include "rocks_db.hpp"

namespace taraxa {

using std::string;
using std::to_string;

FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    address =
        boost::asio::ip::address::from_string(doc.get<std::string>("address"));
    db_accounts_path = doc.get<std::string>("db_accounts_path");
    db_blocks_path = doc.get<std::string>("db_blocks_path");
    db_transactions_path = doc.get<std::string>("db_transactions_path");
    dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");
    block_proposer_threads = doc.get<uint16_t>("block_proposer_threads");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void FullNode::setVerbose(bool verbose) {
  verbose_ = verbose;
  network_->setVerbose(verbose);
  dag_mgr_->setVerbose(verbose);
  db_blks_->setVerbose(verbose);
}

void FullNode::setDebug(bool debug) {
  debug_ = debug;
  network_->setDebug(debug);
}

FullNode::FullNode(boost::asio::io_context &io_context,
                   std::string const &conf_full_node,
                   std::string const &conf_network) try
    : io_context_(io_context),
      conf_full_node_(conf_full_node),
      conf_network_(conf_network),
      conf_(conf_full_node),
      db_accs_(std::make_shared<RocksDb>(conf_.db_accounts_path)),
      db_blks_(std::make_shared<RocksDb>(conf_.db_blocks_path)),
      db_trxs_(std::make_shared<RocksDb>(conf_.db_transactions_path)),
      blk_qu_(std::make_shared<BlockQueue>(1024 /*capacity*/,
                                           2 /* verifer thread*/)),
      network_(std::make_shared<Network>(conf_network)),
      dag_mgr_(std::make_shared<DagManager>(conf_.dag_processing_threads)),
      blk_proposer_(std::make_shared<BlockProposer>(
          conf_.block_proposer_threads, dag_mgr_->getShared())) {
  std::cout << "Taraxa node statred at address: " << conf_.address << " ..."
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
  network_->setFullNode(getShared());
  network_->start();
  dag_mgr_->start();
  blk_qu_->start();
  blk_proposer_->start();
  for (auto i = 0; i < num_block_workers_; ++i) {
    block_workers_.emplace_back([this]() {
      std::string key;
      while (!stopped_) {
        DagBlock blk = blk_qu_->getVerifiedBlock();
        key = blk.getHash().toString();
        LOG(logger_) << "Writing to block db ... " << key << std::endl;
        if (debug_) {
          std::unique_lock<std::mutex> lock(debug_mutex_);
          received_blocks_++;
        }
        db_blks_->put(blk.getHash().toString(), blk.getJsonStr());
        dag_mgr_->addDagBlock(blk, true);
      }
    });
  }
}

void FullNode::stop() {
  stopped_ = true;
  blk_proposer_->stop();
  network_->stop();
}

void FullNode::storeBlock(DagBlock const &blk) {
  blk_qu_->pushUnverifiedBlock(std::move(blk));
}

DagBlock FullNode::getDagBlock(blk_hash_t const &hash) {
  std::string json = db_blks_->get(hash.toString());
  if (json.empty()) {
    return DagBlock();
  } else {
    return DagBlock(json);
  }
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
  DagBlock blk = getDagBlock(hash);
  std::vector<std::string> parents;
  parents.emplace_back(blk.getPivot().toString());

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

FullNodeConfig const &FullNode::getConfig() const { return conf_; }
std::shared_ptr<Network> FullNode::getNetwork() const { return network_; }

}  // namespace taraxa
