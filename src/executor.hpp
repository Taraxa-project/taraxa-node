#ifndef TARAXA_NODE_EXECUTOR_HPP
#define TARAXA_NODE_EXECUTOR_HPP

#include <atomic>

#include "dag.hpp"
#include "final_chain.hpp"
#include "net/WSServer.h"
#include "pbft_chain.hpp"
#include "replay_protection_service.hpp"
#include "transaction_manager.hpp"
#include "util.hpp"
#include "vote.hpp"

namespace taraxa {

class Executor {
 public:
  Executor(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<DagManager> dag_mgr,
           std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
           std::shared_ptr<PbftChain> pbft_chain, uint32_t expected_max_trx_per_block);
  ~Executor();

  void setWSServer(std::shared_ptr<net::WSServer> ws_server);
  void start();
  void stop();
  void run();

  boost::condition_variable_any cv_executor;

 private:
  using uLock = boost::unique_lock<boost::shared_mutex>;

  void executePbftBlocks_();

  unique_ptr<ReplayProtectionService> replay_protection_service_;
  std::shared_ptr<DbStorage> db_ = nullptr;
  std::shared_ptr<DagManager> dag_mgr_ = nullptr;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<PbftChain> pbft_chain_ = nullptr;
  std::shared_ptr<net::WSServer> ws_server_;

  addr_t node_addr_;
  std::atomic<bool> stopped_ = true;
  std::unique_ptr<std::thread> exec_worker_ = nullptr;

  dev::eth::Transactions transactions_tmp_buf_;
  std::atomic<uint64_t> num_executed_blk_ = 0;
  std::atomic<uint64_t> num_executed_trx_ = 0;

  mutable boost::shared_mutex shared_mutex_executor_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa

#endif  // EXECUTOR_H
