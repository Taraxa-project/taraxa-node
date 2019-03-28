/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-02 14:19:58
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 16:25:57
 */

#ifndef FULL_NODE_HPP
#define FULL_NODE_HPP

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "libdevcore/Log.h"
#include "util.hpp"

namespace taraxa {

class RocksDb;
class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class BlockQueue;

struct FullNodeConfig {
  FullNodeConfig(std::string const &json_file);
  std::string json_file_name;
  boost::asio::ip::address address;
  std::string db_accounts_path;
  std::string db_blocks_path;
  std::string db_transactions_path;
  uint16_t dag_processing_threads;
  uint16_t block_proposer_threads;
};

class FullNode : public std::enable_shared_from_this<FullNode> {
 public:
  FullNode(boost::asio::io_context &io_context,
           std::string const &conf_full_node, std::string const &conf_network);
  virtual ~FullNode() {
    if (!stopped_) {
      stop();
    }
  }
  void setVerbose(bool verbose);
  void setDebug(bool debug);
  void start();
  void stop();
  // ** Note can be called only FullNode is fully settled!!!
  std::shared_ptr<FullNode> getShared();
  boost::asio::io_context &getIoContext();

  FullNodeConfig const &getConfig() const;
  std::shared_ptr<Network> getNetwork() const;

  // Store a block in persistent storage and build in dag
  void storeBlock(DagBlock const &blk);

  // Dag query: return childern, siblings, tips before time stamp
  DagBlock getDagBlock(blk_hash_t const &hash);
  time_stamp_t getDagBlockTimeStamp(blk_hash_t const &hash);
  void setDagBlockTimeStamp(blk_hash_t const &hash, time_stamp_t stamp);
  std::vector<std::string> getDagBlockChildren(blk_hash_t const &blk,
                                               time_stamp_t stamp);
  std::vector<std::string> getDagBlockSubtree(blk_hash_t const &blk,
                                              time_stamp_t stamp);
  std::vector<std::string> getDagBlockSiblings(blk_hash_t const &blk,
                                               time_stamp_t stamp);
  std::vector<std::string> getDagBlockTips(blk_hash_t const &blk,
                                           time_stamp_t stamp);
  std::vector<std::string> getDagBlockPivotChain(blk_hash_t const &blk,
                                                 time_stamp_t stamp);
  std::vector<std::string> getDagBlockEpochs(blk_hash_t const &from,
                                             blk_hash_t const &to);

  // debugger
  uint64_t getNumReceivedBlocks();
  uint64_t getNumProposedBlocks();
  std::pair<uint64_t, uint64_t> getNumVerticesInDag();
  std::pair<uint64_t, uint64_t> getNumEdgesInDag();
  void drawGraph(std::string const &dotfile) const;

 private:
  // ** NOTE: io_context must be constructed before Network
  boost::asio::io_context &io_context_;
  // configure files
  std::string conf_full_node_;
  std::string conf_network_;
  size_t num_block_workers_ = 2;
  bool stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  bool verbose_ = false;
  bool debug_ = false;

  // storage
  std::shared_ptr<RocksDb> db_accs_;
  std::shared_ptr<RocksDb> db_blks_;
  std::shared_ptr<RocksDb> db_trxs_;

  // network
  std::shared_ptr<Network> network_;
  // dag
  std::shared_ptr<DagManager> dag_mgr_;
  // ledger
  std::shared_ptr<BlockQueue> blk_qu_;
  // block proposer (multi processing)
  std::shared_ptr<BlockProposer> blk_proposer_;

  std::vector<std::pair<std::string, uint16_t>>
      remotes_;  // neighbors for broadcasting

  std::vector<std::thread> block_workers_;
  // debugger
  std::mutex debug_mutex_;
  uint64_t received_blocks_ = 0;
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "chain")};
  dev::Logger logger_debug_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "chain")};
};

}  // namespace taraxa
#endif
