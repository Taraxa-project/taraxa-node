#ifndef TARAXA_NODE_FULL_NODE_HPP
#define TARAXA_NODE_FULL_NODE_HPP

#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "config.hpp"
#include "database_face_cache.hpp"
#include "executor.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "libdevcrypto/Common.h"
#include "libweb3jsonrpc/WSServer.h"
#include "pbft_chain.hpp"
#include "replay_protection/index.hpp"
#include "transaction.hpp"
#include "transaction_order_manager.hpp"
#include "util.hpp"
#include "util/process_container.hpp"
#include "vote.h"

class Top;

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
  friend class Top;

  explicit FullNode(std::string const &conf_full_node_file,
                    bool destroy_db = false, bool rebuild_network = false);
  explicit FullNode(FullNodeConfig const &conf_full_node,
                    bool destroy_db = false, bool rebuild_network = false);
  void stop();

 public:
  using container = util::process_container::process_container<FullNode>;
  friend container;

  template <typename... Arg>
  static container make(Arg &&... args) {
    return new FullNode(std::forward<Arg>(args)...);
  }

  //  virtual ~FullNode() = default;
  void setDebug(bool debug);
  void start(bool boot_node);
  // ** Note can be called only FullNode is fully settled!!!
  std::shared_ptr<FullNode> getShared();
  boost::asio::io_context &getIoContext();

  FullNodeConfig const &getConfig() const;
  std::shared_ptr<Network> getNetwork() const;
  std::shared_ptr<TransactionManager> getTransactionManager() const {
    return trx_mgr_;
  }

  // master boot node
  addr_t getMasterBootNodeAddress() const { return master_boot_node_address_; }
  void setMasterBootNodeAddress(addr_t const &addr) {
    master_boot_node_address_ = addr;
  }

  // network stuff
  size_t getPeerCount() const;
  std::vector<public_t> getAllPeers() const;

  // Insert a block in persistent storage and build in dag
  void insertBlock(DagBlock const &blk);

  // Only used in initial syncs when blocks are received with full list of
  // transactions
  void insertBroadcastedBlockWithTransactions(
      DagBlock const &blk, std::vector<Transaction> const &transactions);
  // Insert new transaction, critical
  bool insertTransaction(Transaction const &trx);
  // Transactions coming from broadcasting is less critical
  void insertBroadcastedTransactions(
      std::vector<taraxa::bytes> const &transactions);
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransaction(
      trx_hash_t const &hash) const;

  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash) const;
  std::shared_ptr<DagBlock> getDagBlockFromDb(blk_hash_t const &hash) const;
  void updateNonceTable(DagBlock const &dagblk, DagFrontier const &frontier);
  bool isBlockKnown(blk_hash_t const &hash);
  std::vector<std::shared_ptr<DagBlock>> getDagBlocksAtLevel(
      unsigned long level, int number_of_levels);
  std::string getScheduleBlockByPeriod(uint64_t period);
  std::vector<std::string> collectTotalLeaves();
  void getLatestPivotAndTips(std::string &pivot,
                             std::vector<std::string> &tips);
  void getGhostPath(std::string const &source, std::vector<std::string> &ghost);
  void getGhostPath(std::vector<std::string> &ghost);

  std::vector<std::string> getDagBlockPivotChain(blk_hash_t const &blk);
  // Note: returned block hashes does not have order
  // Epoch friends : dag blocks in the same epoch/period
  std::vector<std::string> getDagBlockEpFriend(blk_hash_t const &from,
                                               blk_hash_t const &to);

  // return {period, block order}, for pbft-pivot-blk proposing (does not
  // finialize)
  std::pair<uint64_t, std::shared_ptr<vec_blk_t>> getDagBlockOrder(
      blk_hash_t const &anchor);
  std::pair<bool, uint64_t> getDagBlockPeriod(blk_hash_t const &hash);
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
  std::unordered_set<std::string> getUnOrderedDagBlks() const;
  // get transaction schecules stuff ...
  // fixme: return optional
  blk_hash_t getDagBlockFromTransaction(trx_hash_t const &trx) const {
    return trx_order_mgr_->getDagBlockFromTransaction(trx);
  }

  std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
  computeTransactionOverlapTable(std::shared_ptr<vec_blk_t> ordered_dag_blocks);

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
  bool executeScheduleBlock(ScheduleBlock const &sche_blk,
                            std::unordered_map<addr_t, PbftSortitionAccount>
                                &sortition_account_balance_table,
                            uint64_t period);

  // get DBs
  std::shared_ptr<DatabaseFaceCache> getTrxsDB() const { return db_trxs_; }
  std::shared_ptr<DatabaseFaceCache> getBlksDB() const { return db_blks_; }
  std::shared_ptr<dev::db::DatabaseFace> getTrxsToBlkDB() const {
    return db_trxs_to_blk_;
  }
  std::shared_ptr<account_state::StateRegistry> getStateRegistry() const {
    return state_registry_;
  }
  std::shared_ptr<account_state::State> updateAndGetState() const {
    state_registry_->rebase(*state_);
    return state_;
  }

  std::unordered_map<trx_hash_t, Transaction> getVerifiedTrxSnapShot();
  std::vector<taraxa::bytes> getNewVerifiedTrxSnapShotSerialized();

  // PBFT
  bool shouldSpeak(PbftVoteTypes type, uint64_t period, size_t step);
  dev::Signature signMessage(std::string message);
  bool verifySignature(dev::Signature const &signature, std::string &message);
  std::vector<Vote> getAllVotes();
  bool addVote(Vote const &vote);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  dev::Logger &getTimeLogger() { return log_time_; }
  std::shared_ptr<PbftManager> getPbftManager() const { return pbft_mgr_; }
  bool isKnownPbftBlockForSyncing(blk_hash_t const &pbft_block_hash) const;
  bool isKnownUnverifiedPbftBlock(blk_hash_t const &pbft_block_hash) const;
  uint64_t getPbftChainSize() const;
  void pushUnverifiedPbftBlock(PbftBlock const &pbft_block);
  void setVerifiedPbftBlock(PbftBlock const &pbft_block);
  void newOrderedBlock(blk_hash_t const &dag_block_hash,
                       uint64_t const &block_number);
  void newPendingTransaction(trx_hash_t const &trx_hash);
  void storeCertVotes(blk_hash_t const &pbft_hash,
                      std::vector<Vote> const &votes);
  bool pbftBlockHasEnoughCertVotes(blk_hash_t const &blk_hash,
                                   std::vector<Vote> &votes) const;
  void setTwoTPlusOne(size_t val);
  std::shared_ptr<VoteManager> getVoteManager() const { return vote_mgr_; }
  std::shared_ptr<PbftChain> getPbftChain() const { return pbft_chain_; }
  std::shared_ptr<dev::db::DatabaseFace> getPbftChainDB() const {
    return db_pbftchain_;
  }
  std::shared_ptr<dev::db::DatabaseFace> getPbftBlocksOrderDB() const {
    return db_pbft_blocks_order_;
  }
  std::shared_ptr<dev::db::DatabaseFace> getDagBlocksOrderDB() const {
    return db_dag_blocks_order_;
  }
  std::shared_ptr<dev::db::DatabaseFace> getDagBlocksHeightDB() const {
    return db_dag_blocks_height_;
  }
  std::shared_ptr<dev::db::DatabaseFace> getPbftSortitionAccountsDB() const {
    return db_pbft_sortition_accounts_;
  }
  std::shared_ptr<DatabaseFaceCache> getVotesDB() const {
    return db_cert_votes_;
  }

  std::shared_ptr<dev::db::DatabaseFace> getPeriodScheduleBlockDB() const {
    return db_period_schedule_block_;
  }
  std::shared_ptr<dev::db::DatabaseFace> getDagBlocksPeriodDB() const {
    return db_dag_blocks_period_;
  }

  // PBFT RPC
  void broadcastVote(Vote const &vote);
  Vote generateVote(blk_hash_t const &blockhash, PbftVoteTypes type,
                    uint64_t period, size_t step,
                    blk_hash_t const &last_pbft_block_hash);

  // get dag block for rpc
  std::pair<blk_hash_t, bool> getDagBlockHash(uint64_t dag_block_height) const;
  std::pair<uint64_t, bool> getDagBlockHeight(
      blk_hash_t const &dag_block_hash) const;
  uint64_t getDagBlockMaxHeight() const;

  // For Debug
  uint64_t getNumReceivedBlocks() const;
  uint64_t getNumProposedBlocks() const;
  level_t getMaxDagLevel() const;
  std::pair<uint64_t, uint64_t> getNumVerticesInDag() const;
  std::pair<uint64_t, uint64_t> getNumEdgesInDag() const;
  void drawGraph(std::string const &dotfile) const;
  unsigned long getTransactionStatusCount() const;
  TransactionUnsafeStatusTable getUnsafeTransactionStatusTable() const;
  auto getNumTransactionExecuted() const {
    return executor_->getNumExecutedTrx();
  }
  auto getNumBlockExecuted() const { return executor_->getNumExecutedBlk(); }
  std::vector<blk_hash_t> getLinearizedDagBlocks() const;
  std::vector<trx_hash_t> getPackedTrxs() const;
  void setWSServer(std::shared_ptr<taraxa::WSServer> const &ws_server) {
    ws_server_ = ws_server;
  }

 private:
  size_t num_block_workers_ = 2;
  // fixme (here and everywhere else): non-volatile
  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  bool debug_ = false;
  uint64_t propose_threshold_ = 512;
  bool i_am_boot_node_ = false;
  // node secrets
  secret_t node_sk_;
  public_t node_pk_;
  addr_t node_addr_;
  addr_t master_boot_node_address_;

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
  using ReplayProtectionService = replay_protection::ReplayProtectionService;
  std::shared_ptr<ReplayProtectionService> replay_protection_service_;
  // transaction executor
  std::shared_ptr<Executor> executor_ = nullptr;

  //
  std::vector<std::thread> block_workers_;

  // PBFT
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;

  std::shared_ptr<taraxa::WSServer> ws_server_;
  // storage
  std::shared_ptr<dev::db::RocksDB> db_replay_protection_service_;
  std::shared_ptr<DatabaseFaceCache> db_blks_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_blks_index_ = nullptr;
  std::shared_ptr<DatabaseFaceCache> db_trxs_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_trxs_to_blk_ = nullptr;
  std::shared_ptr<account_state::StateRegistry> state_registry_ = nullptr;
  std::shared_ptr<account_state::State> state_ = nullptr;
  // PBFT DB
  std::shared_ptr<dev::db::DatabaseFace> db_pbft_sortition_accounts_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_pbftchain_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_pbft_blocks_order_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_dag_blocks_order_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_dag_blocks_height_ = nullptr;
  std::shared_ptr<DatabaseFaceCache> db_cert_votes_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_dag_blocks_period_ = nullptr;
  std::shared_ptr<dev::db::DatabaseFace> db_period_schedule_block_ = nullptr;

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
