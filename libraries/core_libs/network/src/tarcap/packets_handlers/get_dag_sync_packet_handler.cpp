#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag_manager.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetDagSyncPacketHandler::GetDagSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                                 const addr_t &node_addr)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, "GET_DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      db_(std::move(db)) {}

void GetDagSyncPacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  if (constexpr size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetDagSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  if (!peer->requestDagSyncingAllowed()) {
    // This should not be possible for honest node
    // Each node should perform dag syncing only when allowed
    std::ostringstream err_msg;
    err_msg << "Received multiple GetDagSyncPackets from " << packet_data.from_node_id_.abridged();

    throw MaliciousPeerException(err_msg.str());
  }

  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  std::unordered_set<blk_hash_t> blocks_hashes;
  auto it = packet_data.rlp_.begin();
  const auto peer_period = (*it++).toInt<PbftPeriod>();

  std::string blocks_hashes_to_log;
  for (const auto block_hash_rlp : *it) {
    blk_hash_t hash = block_hash_rlp.toHash<blk_hash_t>();
    blocks_hashes_to_log += hash.abridged();
    blocks_hashes.emplace(hash);
  }

  LOG(log_dg_) << "Received GetDagSyncPacket: " << blocks_hashes_to_log << " from " << peer->getId();

  auto [period, blocks, transactions] = dag_mgr_->getNonFinalizedBlocksWithTransactions(blocks_hashes);
  if (peer_period == period) {
    peer->syncing_ = false;
    peer->peer_requested_dag_syncing_ = true;
    peer->peer_requested_dag_syncing_time_ =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  } else {
    // There is no point in sending blocks if periods do not match, but an empty packet should be sent
    blocks.clear();
    transactions.clear();
  }
  sendBlocks(packet_data.from_node_id_, std::move(blocks), std::move(transactions), peer_period, period);
}

void GetDagSyncPacketHandler::sendBlocks(const dev::p2p::NodeID &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> &&blocks,
                                         SharedTransactions &&transactions, PbftPeriod request_period,
                                         PbftPeriod period) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  dev::RLPStream s(4);
  s.append(request_period);
  s.append(period);

  s.appendList(transactions.size());
  for (const auto &tx : transactions) {
    s.appendRaw(tx->rlp());
  }

  s.appendList(blocks.size());
  for (const auto &block : blocks) {
    s.appendRaw(block->rlp(true));
  }

  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
