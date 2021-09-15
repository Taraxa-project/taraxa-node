#include "get_blocks_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetBlocksPacketsHandler::GetBlocksPacketsHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr,
                                                 std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                                 std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_BLOCKS_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)) {}

void GetBlocksPacketsHandler::process(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                      const std::shared_ptr<dev::p2p::Host> &host __attribute__((unused)),
                                      const std::shared_ptr<TaraxaPeer> &peer __attribute__((unused))) {
  std::unordered_set<blk_hash_t> blocks_hashes;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  auto it = packet_rlp.begin();
  const auto mode = static_cast<GetBlocksPacketRequestType>((*it++).toInt<unsigned>());

  if (mode == GetBlocksPacketRequestType::MissingHashes)
    LOG(log_dg_) << "Received GetBlocksPacket with " << packet_rlp.itemCount() - 1 << " missing blocks";
  else if (mode == GetBlocksPacketRequestType::KnownHashes)
    LOG(log_dg_) << "Received GetBlocksPacket with " << packet_rlp.itemCount() - 1 << " known blocks";

  for (; it != packet_rlp.end(); ++it) {
    blocks_hashes.emplace(*it);
  }

  const auto &blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      const auto hash = block;
      if (mode == GetBlocksPacketRequestType::MissingHashes) {
        if (blocks_hashes.count(hash) == 1) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      } else if (mode == GetBlocksPacketRequestType::KnownHashes) {
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

  // This means that someone requested more hashes that we actually have -> do not send anything
  if (mode == GetBlocksPacketRequestType::MissingHashes && dag_blocks.size() != blocks_hashes.size()) {
    LOG(log_nf_) << "Node " << packet_data.from_node_id_ << " requested unknown DAG block";
    return;
  }
  sendBlocks(packet_data.from_node_id_, dag_blocks);
}

void GetBlocksPacketsHandler::sendBlocks(dev::p2p::NodeID const &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> blocks) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  std::unordered_map<blk_hash_t, std::vector<taraxa::bytes>> block_transactions;
  size_t total_transactions_count = 0;
  for (const auto &block : blocks) {
    std::vector<taraxa::bytes> transactions;
    for (auto trx : block->getTrxs()) {
      auto t = trx_mgr_->getTransaction(trx);
      if (!t) {
        LOG(log_er_) << "Transacation " << trx << " is not available. SendBlocks canceled";
        // TODO: This can happen on stopping the node because network
        // is not stopped since network does not support restart,
        // better solution needed
        return;
      }
      transactions.emplace_back(std::move(t->second));
      total_transactions_count++;
    }
    LOG(log_nf_) << "Send DagBlock " << block->getHash() << "# Trx: " << transactions.size() << std::endl;
    block_transactions[block->getHash()] = std::move(transactions);
  }

  RLPStream s(blocks.size() + total_transactions_count);
  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
    taraxa::bytes trx_bytes;
    for (auto &trx : block_transactions[block->getHash()]) {
      trx_bytes.insert(trx_bytes.end(), std::make_move_iterator(trx.begin()), std::make_move_iterator(trx.end()));
    }
    s.appendRaw(trx_bytes, block_transactions[block->getHash()].size());
  }
  sealAndSend(peer_id, BlocksPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap