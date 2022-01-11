#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
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

void GetDagSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  std::unordered_set<blk_hash_t> blocks_hashes;
  auto it = packet_data.rlp_.begin();
  const auto peer_period = (*it++).toInt<uint64_t>();

  LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " known blocks";

  for (; it != packet_data.rlp_.end(); ++it) {
    blocks_hashes.emplace(*it);
  }

  dag_mgr_->sendNonFinalizedBlocks(std::move(blocks_hashes), std::move(peer), peer_period);
}

void GetDagSyncPacketHandler::sendBlocks(const dev::p2p::NodeID &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> &&blocks, uint64_t request_period,
                                         uint64_t period) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  size_t total_transactions_count = 0;
  std::unordered_set<trx_hash_t> unique_trxs;
  std::vector<taraxa::bytes> transactions;
  std::string dag_blocks_to_send;
  for (const auto &block : blocks) {
    std::vector<trx_hash_t> trx_to_query;
    for (auto trx : block->getTrxs()) {
      if (unique_trxs.emplace(trx).second) {
        trx_to_query.emplace_back(trx);
      }
    }
    auto trxs = db_->getNonfinalizedTransactions(trx_to_query);

    for (auto t : trxs) {
      transactions.emplace_back(std::move(*t->rlp()));
      total_transactions_count++;
    }
    dag_blocks_to_send += block->getHash().abridged();
  }

  dev::RLPStream s(3 + blocks.size() + total_transactions_count);
  s << static_cast<uint64_t>(request_period);
  s << static_cast<uint64_t>(period);
  s << transactions.size();
  taraxa::bytes trx_bytes;
  for (auto &trx : transactions) {
    trx_bytes.insert(trx_bytes.end(), std::make_move_iterator(trx.begin()), std::make_move_iterator(trx.end()));
  }
  s.appendRaw(trx_bytes, transactions.size());

  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
  }
  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
  LOG(log_nf_) << "Send DagSyncPacket with " << dag_blocks_to_send << "# Trx: " << transactions.size() << std::endl;
}

}  // namespace taraxa::network::tarcap
