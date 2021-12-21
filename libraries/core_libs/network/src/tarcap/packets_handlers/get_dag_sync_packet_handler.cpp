#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
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
  std::unordered_set<blk_hash_t> blocks_hashes;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  auto it = packet_data.rlp_.begin();
  const auto mode = static_cast<DagSyncRequestType>((*it++).toInt<unsigned>());

  if (mode == DagSyncRequestType::MissingHashes)
    LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " missing blocks";
  else if (mode == DagSyncRequestType::KnownHashes)
    LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " known blocks";

  for (; it != packet_data.rlp_.end(); ++it) {
    blocks_hashes.emplace(*it);
  }

  uint32_t block_count = 0;

  static const uint32_t max_blocks_to_send = 100;

  const auto &blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      if (block_count == max_blocks_to_send) break;
      const auto hash = block;
      if (mode == DagSyncRequestType::MissingHashes) {
        if (blocks_hashes.count(hash) == 1) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
            block_count++;
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      } else if (mode == DagSyncRequestType::KnownHashes) {
        if (blocks_hashes.count(hash) == 0) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
            block_count++;
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      }
    }
  }

  // This means that someone requested more hashes that we actually have -> do not send anything
  if (mode == DagSyncRequestType::MissingHashes && dag_blocks.size() != blocks_hashes.size()) {
    LOG(log_nf_) << "Node " << packet_data.from_node_id_ << " requested unknown DAG block";
    return;
  }
  sendBlocks(packet_data.from_node_id_, dag_blocks);
}

void GetDagSyncPacketHandler::sendBlocks(dev::p2p::NodeID const &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> blocks) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  std::unordered_map<blk_hash_t, std::vector<taraxa::bytes>> block_transactions;
  size_t total_transactions_count = 0;
  std::unordered_set<trx_hash_t> unique_trxs;
  for (const auto &block : blocks) {
    std::vector<taraxa::bytes> transactions;
    std::vector<trx_hash_t> trx_to_query;
    for (auto trx : block->getTrxs()) {
      if (unique_trxs.emplace(trx).second) {
        trx_to_query.emplace_back(trx);
      }
    }
    auto trxs = db_->getTransactions(block->getTrxs());

    for (auto t : trxs) {
      transactions.emplace_back(std::move(*t->rlp()));
      total_transactions_count++;
    }

    LOG(log_nf_) << "Send DagBlock " << block->getHash() << "# Trx: " << transactions.size() << std::endl;
    block_transactions[block->getHash()] = std::move(transactions);
  }

  dev::RLPStream s(blocks.size() + total_transactions_count);
  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
    taraxa::bytes trx_bytes;
    for (auto &trx : block_transactions[block->getHash()]) {
      trx_bytes.insert(trx_bytes.end(), std::make_move_iterator(trx.begin()), std::make_move_iterator(trx.end()));
    }
    s.appendRaw(trx_bytes, block_transactions[block->getHash()].size());
  }
  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
