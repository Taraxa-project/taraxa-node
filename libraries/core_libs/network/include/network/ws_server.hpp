#pragma once

#include <vector>

#include "common/thread_pool.hpp"
#include "dag/dag_block.hpp"
#include "final_chain/data.hpp"
#include "network/ws_session.hpp"
#include "pbft/pbft_chain.hpp"
#include "pillar_chain/pillar_block.hpp"

namespace taraxa::net {

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WsServer : public std::enable_shared_from_this<WsServer>, public jsonrpc::AbstractServerConnector {
 public:
  WsServer(std::shared_ptr<util::ThreadPool> thread_pool, tcp::endpoint endpoint, addr_t node_addr,
           uint32_t max_pending_tasks);
  virtual ~WsServer();

  WsServer(const WsServer&) = delete;
  WsServer(WsServer&&) = delete;
  WsServer& operator=(const WsServer&) = delete;
  WsServer& operator=(WsServer&&) = delete;

  // Start accepting incoming connections
  void run();
  void newEthBlock(const ::taraxa::final_chain::BlockHeader& payload, const TransactionHashes& trx_hashes);
  void newDagBlock(const std::shared_ptr<DagBlock>& blk);
  void newDagBlockFinalized(const blk_hash_t& blk, uint64_t period);
  void newPbftBlockExecuted(const PbftBlock& blk, const std::vector<blk_hash_t>& finalized_dag_blk_hashes);
  void newPendingTransaction(const trx_hash_t& trx_hash);
  void newPillarBlockData(const pillar_chain::PillarBlockData& pillar_block_data);
  uint32_t numberOfSessions();
  uint32_t numberOfPendingTasks() const;
  bool pendingTasksOverLimit() const { return numberOfPendingTasks() > kMaxPendingTasks; }

  virtual std::shared_ptr<WsSession> createSession(tcp::socket&& socket) = 0;

  virtual bool StartListening() { return true; }
  virtual bool StopListening() { return true; }

 private:
  void do_accept();
  void on_accept(beast::error_code ec, tcp::socket socket);
  LOG_OBJECTS_DEFINE
  boost::asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<WsSession>> sessions_;
  std::atomic<bool> stopped_ = false;
  boost::shared_mutex sessions_mtx_;

 protected:
  std::weak_ptr<util::ThreadPool> thread_pool_;
  uint32_t kMaxPendingTasks;
  const addr_t node_addr_;
};

}  // namespace taraxa::net
