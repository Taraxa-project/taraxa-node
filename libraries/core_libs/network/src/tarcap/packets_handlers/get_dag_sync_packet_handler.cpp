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

void GetDagSyncPacketHandler::process(const PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  std::unordered_set<blk_hash_t> blocks_hashes;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  auto it = packet_data.rlp_.begin();
  const auto peer_period = (*it++).toInt<uint64_t>();

  std::string blocks_hashes_to_log;
  for (; it != packet_data.rlp_.end(); ++it) {
    blk_hash_t hash(*it);
    blocks_hashes_to_log += hash.abridged();
    blocks_hashes.emplace(std::move(hash));
  }

  LOG(log_nf_) << "Received GetDagSyncPacket: " << blocks_hashes_to_log << " from " << peer->getId();

  const auto [period, blocks] = dag_mgr_->getNonFinalizedBlocks();
  // There is no point in sending blocks if periods do not match
  if (peer_period == period) {
    peer->syncing_ = false;
    for (auto &level_blocks : blocks) {
      for (auto &block : level_blocks.second) {
        const auto hash = block;
        if (blocks_hashes.count(hash) == 0) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      }
    }
  }
  sendBlocks(packet_data.from_node_id_, std::move(dag_blocks), peer_period, period);
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
  std::string transactions_to_log;
  for (const auto &block : blocks) {
    std::vector<trx_hash_t> trx_to_query;
    for (auto trx : block->getTrxs()) {
      if (unique_trxs.emplace(trx).second) {
        trx_to_query.emplace_back(trx);
      }
    }
    auto trxs = db_->getNonfinalizedTransactions(trx_to_query);

    for (auto t : trxs) {
      transactions_to_log += t->getHash().abridged();
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
  LOG(log_nf_) << "Send DagSyncPacket with " << dag_blocks_to_send << "# Trx: " << transactions_to_log << " from "
               << peer->getId();
}

}  // namespace taraxa::network::tarcap
