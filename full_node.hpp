/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-02 14:19:58
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 17:48:07
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
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"
#include "libdevcore/OverlayDB.h"
#include "libethereum/State.h"

namespace taraxa {

class Network;
class BlockProposer;
class DagManager;
class DagBlock;
class BlockQueue;
class Transaction;
class TransactionManager;
class Executor;
class Vote;
class VoteQueue;
class PbftManager;
class NetworkConfig;

class FullNode : public std::enable_shared_from_this<FullNode> {
 public:
  explicit FullNode(boost::asio::io_context &io_context,
                    std::string const &conf_full_node_file);
  explicit FullNode(boost::asio::io_context &io_context,
                    FullNodeConfig const &conf_full_node);

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

  // network stuff
  size_t getPeerCount() const;
  std::vector<public_t> getAllPeers() const;
  // Store a block in persistent storage and build in dag
  void storeBlock(DagBlock const &blk);
  void storeBlockAndSign(DagBlock const &blk);

  // Only used in initial syncs when blocks are received with full list of
  // transactions
  void storeBlockWithTransactions(DagBlock const &blk,
                                  std::vector<Transaction> const &transactions);

  // Store transaction
  void storeTransaction(Transaction const &trx);

  // Dag query: return childern, siblings, tips before time stamp
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash);

  bool isBlockKnown(blk_hash_t const &hash);
  time_stamp_t getDagBlockTimeStamp(blk_hash_t const &hash);
  void setDagBlockTimeStamp(blk_hash_t const &hash, time_stamp_t stamp);
  std::vector<std::string> getDagBlockChildren(blk_hash_t const &blk,
                                               time_stamp_t stamp);
  std::vector<std::string> getTotalDagBlockChildren(blk_hash_t const &blk,
                                                    time_stamp_t stamp);
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
  std::vector<std::string> getDagBlockEpochs(blk_hash_t const &from,
                                             blk_hash_t const &to);

  // account stuff
  std::pair<bal_t, bool> getBalance(addr_t const &acc) const;
  bool setBalance(addr_t const &acc, bal_t const &new_bal);
  addr_t getAddress();

  // pbft stuff
  bool executeScheduleBlock(ScheduleBlock const &sche_blk);
  // debugger
  uint64_t getNumReceivedBlocks();
  uint64_t getNumProposedBlocks();
  std::pair<uint64_t, uint64_t> getNumVerticesInDag();
  std::pair<uint64_t, uint64_t> getNumEdgesInDag();
  void drawGraph(std::string const &dotfile) const;

  std::unordered_map<trx_hash_t, Transaction> getNewVerifiedTrxSnapShot(
      bool onlyNew);
  void insertNewTransactions(
      std::unordered_map<trx_hash_t, Transaction> const &transactions);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash);

  // PBFT
  bool shouldSpeak(blk_hash_t const &blockhash, char type, int period,
                   int step);
  dev::Signature signMessage(std::string message);
  bool verifySignature(dev::Signature const &signature, std::string &message);
  void placeVote(blk_hash_t const &blockhash, char type, int period, int step);
  std::vector<Vote> getVotes(int period);
  void placeVote(Vote const &vote);
  void broadcastVote(taraxa::blk_hash_t const &blockhash, char type, int period,
                     int step);
  void clearVoteQueue();
  size_t getVoteQueueSize();
  bool isKnownVote(taraxa::Vote const &vote) const;
  void setVoteKnown(taraxa::Vote const &vote);
  dev::Logger &getTimeLogger() { return log_time_; }

 private:
  // ** NOTE: io_context must be constructed before Network
  boost::asio::io_context &io_context_;
  size_t num_block_workers_ = 2;
  bool stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  bool verbose_ = false;
  bool debug_ = false;

  // node secrets
  secret_t node_sk_;
  public_t node_pk_;
  addr_t node_addr_;

  // storage for schedule trx; TODO: merge it into State class
  dev::OverlayDB db;
  // storage for EVM state related
  dev::eth::State state;

  // network
  std::shared_ptr<Network> network_;
  // dag
  std::shared_ptr<DagManager> dag_mgr_;
  // ledger
  std::shared_ptr<BlockQueue> blk_qu_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  // block proposer (multi processing)
  std::shared_ptr<BlockProposer> blk_proposer_;

  // transaction executor
  std::shared_ptr<Executor> executor_;
  //
  std::vector<std::thread> block_workers_;

  // PBFT
  std::shared_ptr<VoteQueue> vote_queue_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::unordered_set<sig_hash_t> known_votes_;

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
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TIME")};
};

}  // namespace taraxa
#endif
