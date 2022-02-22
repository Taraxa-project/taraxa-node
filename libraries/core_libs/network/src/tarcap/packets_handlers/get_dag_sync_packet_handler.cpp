#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetDagSyncPacketHandler::GetDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr,
                                                 std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                                 std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)) {}

void GetDagSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) {
  checkPacketRlpList(packet_data);

  if (size_t required_min_size = 1; packet_data.rlp_.itemCount() < required_min_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_min_size);
  }

  // TODO: rlp format of this packet should be fixed:
  //       has format: [peer_period, blk_hash1, ..., blk_hashN]
  //       should have format: [peer_period, [blk_hash1, ..., blk_hashN] ]

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void GetDagSyncPacketHandler::process(const PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  if (peer->peer_requested_dag_syncing_) {
    // This should not be possible for honest node
    // Each node should perform dag syncing only once
    // This should not be possible for honest node
    std::ostringstream err_msg;
    err_msg << "Received multiple GetDagSyncPackets from " << packet_data.from_node_id_.abridged();

    throw MaliciousPeerException(err_msg.str());
  }

  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  std::unordered_set<blk_hash_t> blocks_hashes;
  auto it = packet_data.rlp_.begin();
  const auto peer_period = (*it++).toInt<uint64_t>();

  std::string blocks_hashes_to_log;
  for (; it != packet_data.rlp_.end(); ++it) {
    blk_hash_t hash = (*it).toHash<blk_hash_t>();
    blocks_hashes_to_log += hash.abridged();
    blocks_hashes.emplace(std::move(hash));
  }

  LOG(log_dg_) << "Received GetDagSyncPacket: " << blocks_hashes_to_log << " from " << peer->getId();

  auto [period, blocks, transactions] = dag_mgr_->getNonFinalizedBlocksWithTransactions(blocks_hashes);
  if (peer_period == period) {
    peer->syncing_ = false;
    peer->peer_requested_dag_syncing_ = true;
  } else {
    // There is no point in sending blocks if periods do not match, but an empty packet should be sent
    blocks.clear();
    transactions.clear();
  }
  sendBlocks(packet_data.from_node_id_, std::move(blocks), std::move(transactions), peer_period, period);
}

void GetDagSyncPacketHandler::sendBlocks(const dev::p2p::NodeID &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> &&blocks,
                                         SharedTransactions &&transactions, uint64_t request_period, uint64_t period) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  size_t total_transactions_count = 0;
  std::vector<taraxa::bytes> raw_transactions;
  std::string dag_blocks_to_send;
  std::string transactions_to_log;

  for (const auto &block : blocks) {
    dag_blocks_to_send += block->getHash().abridged();
  }

  for (const auto &t : transactions) {
    transactions_to_log += t->getHash().abridged();
    raw_transactions.push_back(t->rlp());
    total_transactions_count++;
  }

  dev::RLPStream s(3 + blocks.size() + total_transactions_count);
  s << static_cast<uint64_t>(request_period);
  s << static_cast<uint64_t>(period);
  s << raw_transactions.size();
  taraxa::bytes trx_bytes;
  for (auto &trx : raw_transactions) {
    trx_bytes.insert(trx_bytes.end(), std::make_move_iterator(trx.begin()), std::make_move_iterator(trx.end()));
  }
  s.appendRaw(trx_bytes, raw_transactions.size());

  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
  }

  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
  LOG(log_dg_) << "Send DagSyncPacket with " << dag_blocks_to_send << "# Trx: " << transactions_to_log << " from "
               << peer->getId();
}

}  // namespace taraxa::network::tarcap
