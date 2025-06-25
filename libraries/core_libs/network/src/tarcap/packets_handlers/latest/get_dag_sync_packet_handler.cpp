#include "network/tarcap/packets_handlers/latest/get_dag_sync_packet_handler.hpp"

#include "dag/dag_manager.hpp"
#include "network/tarcap/packets/latest/dag_sync_packet.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetDagSyncPacketHandler::GetDagSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                                 const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), logs_prefix + "GET_DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      db_(std::move(db)) {}

void GetDagSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<GetDagSyncPacket>(packet_data.rlp_);

  if (!peer->requestDagSyncingAllowed()) {
    // This should not be possible for honest node
    // Each node should perform dag syncing only when allowed
    std::ostringstream err_msg;
    err_msg << "Received multiple GetDagSyncPackets from " << peer->getId().abridged();

    throw MaliciousPeerException(err_msg.str());
  }

  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  std::unordered_set<blk_hash_t> blocks_hashes_set;
  std::string blocks_hashes_to_log;
  blocks_hashes_to_log.reserve(packet.blocks_hashes.size());
  for (const auto &hash : packet.blocks_hashes) {
    if (blocks_hashes_set.insert(hash).second) {
      blocks_hashes_to_log += hash.abridged();
    }
  }

  logger_->debug("Received GetDagSyncPacket: {} from {}", blocks_hashes_to_log, peer->getId());

  auto [period, blocks, transactions] = dag_mgr_->getNonFinalizedBlocksWithTransactions(blocks_hashes_set);
  if (packet.peer_period == period) {
    peer->syncing_ = false;
    peer->peer_requested_dag_syncing_ = true;
    peer->peer_requested_dag_syncing_time_ =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  } else {
    // There is no point in sending blocks if periods do not match, but an empty packet should be sent
    blocks.clear();
    transactions.clear();
  }
  sendBlocks(peer->getId(), std::move(blocks), std::move(transactions), packet.peer_period, period);
}

void GetDagSyncPacketHandler::sendBlocks(const dev::p2p::NodeID &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> &&blocks,
                                         SharedTransactions &&transactions, PbftPeriod request_period,
                                         PbftPeriod period) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  DagSyncPacket dag_sync_packet(request_period, period, std::move(transactions), std::move(blocks));
  sealAndSend(peer_id, SubprotocolPacketType::kDagSyncPacket, encodePacketRlp(dag_sync_packet));
}

}  // namespace taraxa::network::tarcap
