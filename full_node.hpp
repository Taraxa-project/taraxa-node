/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-02 14:19:58
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:13:09
 */

#ifndef FULL_NODE_HPP
#define FULL_NODE_HPP

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "config.hpp"
#include "executor.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "pbft_chain.hpp"
#include "transaction_order_manager.hpp"
#include "util.hpp"
#include "vote.h"

namespace taraxa {

class RocksDb;
class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class BlockManager;
class Transaction;
class TransactionManager;
class Vote;
class VoteManager;
class PbftManager;
class NetworkConfig;

class FullNode : public std::enable_shared_from_this<FullNode> {
 public:
  explicit FullNode(boost::asio::io_context &io_context,
                    std::string const &conf_full_node_file,
                    bool destroy_db = false, bool rebuild_network = false);
  explicit FullNode(boost::asio::io_context &io_context,
                    FullNodeConfig const &conf_full_node,
                    bool destroy_db = false, bool rebuild_network = false);

  virtual ~FullNode() {
    if (!stopped_) {
      stop();
    }
  }
  void setVerbose(bool verbose);
  void setDebug(bool debug);
  void start(bool boot_node);
  void stop();
  bool reset();  // clean db, reset everything ... can be called only stopped
  void initDB(bool destroy_db);
  // ** Note can be called only FullNode is fully settled!!!
  std::shared_ptr<FullNode> getShared();
  boost::asio::io_context &getIoContext();

  FullNodeConfig const &getConfig() const;
  std::shared_ptr<Network> getNetwork() const;
  std::shared_ptr<TransactionManager> getTransactionManager() const {
    return trx_mgr_;
  }

  // master boot node
  addr_t getMasterBootNodeAddress() const { return master_boot_node_address; }
  void setMasterBootNodeAddress(addr_t const &addr) {
    master_boot_node_address = addr;
  }

  // network stuff
  size_t getPeerCount() const;
  std::vector<public_t> getAllPeers() const;

  // Insert a block in persistent storage and build in dag
  void insertBlock(DagBlock const &blk);
  void insertBlockAndSign(DagBlock const &blk);

  // Only used in initial syncs when blocks are received with full list of
  // transactions
  void insertBroadcastedBlockWithTransactions(
      DagBlock const &blk, std::vector<Transaction> const &transactions);
  // Insert new transaction, critical
  void insertTransaction(Transaction const &trx);
  // Transactions coming from broadcasting is less critical
  void insertBroadcastedTransactions(
      std::unordered_map<trx_hash_t, Transaction> const &transactions);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash);
  unsigned long getTransactionStatusCount();

  // Dag related: return childern, siblings, tips before time stamp
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash);
  std::shared_ptr<DagBlock> getDagBlockFromDb(blk_hash_t const &hash);

  bool isBlockKnown(blk_hash_t const &hash);
  time_stamp_t getDagBlockTimeStamp(blk_hash_t const &hash);
  void setDagBlockTimeStamp(blk_hash_t const &hash, time_stamp_t stamp);
  std::vector<std::string> getDagBlockChildren(blk_hash_t const &blk,
                                               time_stamp_t stamp);
  std::vector<std::string> getTotalDagBlockChildren(blk_hash_t const &blk,
                                                    time_stamp_t stamp);
  std::vector<std::shared_ptr<DagBlock>> getDagBlocksAtLevel(
      unsigned long level, int number_of_levels);
  std::vector<std::string> collectTotalLeaves();
  void getLatestPivotAndTips(std::string &pivot,
                             std::vector<std::string> &tips);
  void getGhostPath(std::string const &source, std::vector<std::string> &ghost);
  std::vector<std::string> getDagBlockSubtree(blk_hash_t const &blk,
                                              time_stamp_t stamp);
  std::vector<std::string> getDagBlockSiblings(blk_hash_t const &blk,
                                               time_stamp_t stamp);
  std::vector<std::string> getDagBlockTips(blk_hash_t const &blk,
                                           time_stamp_t stamp);
  std::vector<std::string> getDagBlockPivotChain(blk_hash_t const &blk,
                                                 time_stamp_t stamp);
  // Note: returned block hashes does not have order
  // Epoch friends : dag blocks in the same epoch/period
  std::vector<std::string> getDagBlockEpFriend(blk_hash_t const &from,
                                               blk_hash_t const &to);

  // return {period, block order}, for pbft-pivot-blk proposing (does not
  // finialize)
  std::pair<uint64_t, std::shared_ptr<vec_blk_t>> getDagBlockOrder(
      blk_hash_t const &anchor);
  // receive pbft-povit-blk, update periods and finalized, return size of
  // ordered blocks
  uint setDagBlockOrder(blk_hash_t const &anchor, uint64_t period);
  uint64_t getLatestPeriod() const;
  blk_hash_t getLatestAnchor() const;
  uint getBlockProposeThresholdBeta()
      const { /*TODO: should receive it from pbft-block, use threshold from
                 0 ~ 1024 */
    return propose_threshold_;
  }
  void setBlockProposeThresholdBeta(
      uint threshold) { /*TODO: should receive it from pbft-block, use threshold
                           from 0 ~ 1024, if larger than 4096, should always
                           success */
    if (threshold >= (1u << 20)) {
      threshold = (1u << 20);
    }
    propose_threshold_ = threshold;
    LOG(log_wr_) << "Set propose threshold beta to " << threshold;
  }

  // get transaction schecules stuff ...
  blk_hash_t getDagBlockFromTransaction(trx_hash_t const &trx) const {
    return trx_order_mgr_->getDagBlockFromTransaction(trx);
  }

  std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
  computeTransactionOverlapTablelapTable(
      std::shared_ptr<vec_blk_t> ordered_dag_blocks);

  std::vector<std::vector<uint>> createMockTrxSchedule(
      std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
          trx_overlap_table);

  // account stuff
  std::pair<val_t, bool> getBalance(addr_t const &acc) const;
  val_t getMyBalance() const;
  addr_t getAddress() const;
  public_t getPublicKey() const { return node_pk_; }
  secret_t getSecretKey() const { return node_sk_; }
  // pbft stuff
  bool executeScheduleBlock(
      ScheduleBlock const &sche_blk,
      std::unordered_map<addr_t, val_t> &sortition_account_balance_table);
  // debugger
  uint64_t getNumReceivedBlocks();
  uint64_t getNumProposedBlocks();
  level_t getMaxDagLevel() const;
  std::pair<uint64_t, uint64_t> getNumVerticesInDag();
  std::pair<uint64_t, uint64_t> getNumEdgesInDag();
  void drawGraph(std::string const &dotfile) const;
  bool destroyDB();

  // get DBs
  std::shared_ptr<SimpleDBFace> getTrxsDB() const { return db_trxs_; }
  std::shared_ptr<SimpleDBFace> getBlksDB() const { return db_blks_; }
  std::shared_ptr<SimpleDBFace> getTrxsToBlkDB() const {
    return db_trxs_to_blk_;
  }
  std::shared_ptr<StateRegistry::State> updateAndGetState() const {
    state_registry_->rebase(*state_);
    return state_;
  }

  std::unordered_map<trx_hash_t, Transaction> getNewVerifiedTrxSnapShot(
      bool onlyNew);

  // Get max level
  unsigned long getDagMaxLevel() { return max_dag_level_; }

  // PBFT
  bool shouldSpeak(PbftVoteTypes type, uint64_t period, size_t step);
  dev::Signature signMessage(std::string message);
  bool verifySignature(dev::Signature const &signature, std::string &message);
  std::vector<Vote> getVotes(uint64_t round);
  void addVote(Vote const &vote);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  bool isKnownVote(uint64_t pbft_round, vote_hash_t const &vote_hash) const;
  dev::Logger &getTimeLogger() { return log_time_; }
  std::shared_ptr<PbftManager> getPbftManager() const { return pbft_mgr_; }
  bool isKnownPbftBlockInChain(blk_hash_t const &pbft_block_hash) const;
  bool isKnownPbftBlockInQueue(blk_hash_t const &pbft_block_hash) const;
  size_t getPbftChainSize() const;
  size_t getPbftQueueSize() const;
  void pushPbftBlockIntoQueue(PbftBlock const &pbft_block);
  size_t getEpoch() const;
  bool setPbftBlock(PbftBlock const &pbft_block);  // Test purpose
  std::shared_ptr<VoteManager> getVoteManager() const { return vote_mgr_; }
  std::shared_ptr<PbftChain> getPbftChain() const { return pbft_chain_; }
  std::shared_ptr<SimpleDBFace> getVotesDB() const { return db_votes_; }
  std::shared_ptr<SimpleDBFace> getPbftChainDB() const { return db_pbftchain_; }
  std::pair<blk_hash_t, bool> getDagBlockHash(uint64_t dag_block_height) const;
  std::pair<uint64_t, bool> getDagBlockHeight(
      blk_hash_t const &dag_block_hash) const;
  uint64_t getDagBlockMaxHeight() const;
  // PBFT RPC
  void broadcastVote(Vote const &vote);
  Vote generateVote(blk_hash_t const &blockhash, PbftVoteTypes type,
                    uint64_t period, size_t step);

 private:
  // ** NOTE: io_context must be constructed before Network
  boost::asio::io_context &io_context_;
  size_t num_block_workers_ = 2;
  bool stopped_ = true;
  bool db_inited_ = false;
  // configuration
  FullNodeConfig conf_;
  bool verbose_ = false;
  bool debug_ = false;
  uint64_t propose_threshold_ = 512;
  bool i_am_boot_node_ = false;
  // node secrets
  secret_t node_sk_;
  public_t node_pk_;
  addr_t node_addr_;
  addr_t master_boot_node_address;

  // storage
  std::shared_ptr<SimpleDBFace> db_blks_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_blks_index_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_trxs_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_trxs_to_blk_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_votes_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_pbftchain_ = nullptr;
  std::shared_ptr<StateRegistry> state_registry_ = nullptr;
  std::shared_ptr<StateRegistry::State> state_ = nullptr;

  // DAG max level
  unsigned long max_dag_level_ = 0;

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

  // transaction executor
  std::shared_ptr<Executor> executor_ = nullptr;
  //
  std::vector<std::thread> block_workers_;

  // PBFT
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  // TODO: need to shrink later
  // TODO: need to shrink later
  std::unordered_set<vote_hash_t> known_votes_;  // per node itself

  // debugger
  std::mutex debug_mutex_;
  uint64_t received_blocks_ = 0;
  uint64_t received_trxs_ = 0;
  mutable dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "FULLND")};
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "FULLND")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "FULLND")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "FULLND")};
  mutable dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "FULLND")};
  mutable dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "FULLND")};
  mutable dev::Logger log_time_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TMSTM")};
  mutable dev::Logger log_time_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TMSTM")};
};

}  // namespace taraxa
#endif
