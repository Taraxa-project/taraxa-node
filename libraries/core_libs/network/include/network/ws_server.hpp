#pragma once

#include <vector>

#include "dag/dag_block.hpp"
#include "final_chain/data.hpp"
#include "metrics/jsonrpc_metrics.hpp"
#include "network/ws_session.hpp"
#include "pbft/pbft_chain.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::net {
class WsServer : public std::enable_shared_from_this<WsServer>, public jsonrpc::AbstractServerConnector {
 public:
  WsServer(boost::asio::io_context& ioc, tcp::endpoint endpoint, addr_t node_addr,
           std::shared_ptr<metrics::JsonRpcMetrics> metrics);
  virtual ~WsServer();

  WsServer(const WsServer&) = delete;
  WsServer(WsServer&&) = delete;
  WsServer& operator=(const WsServer&) = delete;
  WsServer& operator=(WsServer&&) = delete;

  // Start accepting incoming connections
  void run();
  void newEthBlock(const ::taraxa::final_chain::BlockHeader& payload, const TransactionHashes& trx_hashes);
  void newLogs(const ::taraxa::final_chain::BlockHeader& payload, TransactionHashes trx_hashes,
               const TransactionReceipts& receipts);
  void newDagBlock(const std::shared_ptr<DagBlock>& blk);
  void newDagBlockFinalized(const blk_hash_t& blk, uint64_t period);
  void newPbftBlockExecuted(const PbftBlock& blk, const std::vector<blk_hash_t>& finalized_dag_blk_hashes);
  void newPendingTransaction(const trx_hash_t& trx_hash);
  void newPillarBlockData(const pillar_chain::PillarBlockData& pillar_block_data);
  uint32_t numberOfSessions();

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
  const addr_t node_addr_;
  std::shared_ptr<metrics::JsonRpcMetrics> metrics_;
  friend WsSession;
};

}  // namespace taraxa::net
