#include "get_blocks_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetBlocksPacketsHandler::GetBlocksPacketsHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                                 const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_BLOCKS_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
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
          if (auto blk = db_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      } else if (mode == GetBlocksPacketRequestType::KnownHashes) {
        if (blocks_hashes.count(hash) == 0) {
          if (auto blk = db_->getDagBlock(hash); blk) {
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
  if (blocks.empty()) return;

  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  taraxa::bytes packet_bytes;
  size_t packet_items_count = 0;
  size_t blocks_counter = 0;

  for (auto &block : blocks) {
    size_t dag_block_items_count = 0;
    const size_t previous_block_packet_size = packet_bytes.size();

    // Add dag block rlp to the sent bytes array
    taraxa::bytes block_bytes = block->rlp(true);
    packet_bytes.insert(packet_bytes.end(), std::begin(block_bytes), std::end(block_bytes));
    dag_block_items_count++;  // + 1 new dag blog

    for (auto trx : block->getTrxs()) {
      auto t = trx_mgr_->getTransaction(trx);
      if (!t) {
        LOG(log_er_) << "Transacation " << trx << " is not available. SendBlocks canceled";
        // TODO: This can happen on stopping the node because network
        // is not stopped since network does not support restart,
        // better solution needed
        return;
      }

      // Add dag block transaction rlp to the sent bytes array
      packet_bytes.insert(packet_bytes.end(), std::begin(t->second), std::end(t->second));
      dag_block_items_count++;  // + 1 new tx from dag blog
    }

    LOG(log_tr_) << "Send DagBlock " << block->getHash() << "Trxs count: " << block->getTrxs().size();

    // Split packet into multiple smaller ones if total size is > MAX_PACKET_SIZE
    if (packet_bytes.size() > MAX_PACKET_SIZE) {
      LOG(log_dg_) << "Sending partial BlocksPacket due to MAX_PACKET_SIZE limit. " << blocks_counter
                   << " blocks out of " << blocks.size() << " PbftBlockPacketsent.";

      taraxa::bytes removed_bytes;
      std::copy(packet_bytes.begin() + previous_block_packet_size, packet_bytes.end(),
                std::back_inserter(removed_bytes));
      packet_bytes.resize(previous_block_packet_size);

      RLPStream s(packet_items_count + 1 /* final packet flag */);
      // As DagBlocksPacket might be split into multiple packets, we
      // need to differentiate if is the last one or not due to syncing
      s.append(false);  // flag if it is the final DagBlocksSyncPacket or not
      s.appendRaw(packet_bytes, packet_items_count);
      sealAndSend(peer_id, BlocksPacket, std::move(s));

      packet_bytes = std::move(removed_bytes);
      packet_items_count = 0;
    }

    packet_items_count += dag_block_items_count;
    blocks_counter++;
  }

  LOG(log_dg_) << "Sending final BlocksPacket with " << blocks_counter << " blocks.";

  RLPStream s(packet_items_count + 1 /* final packet flag */);
  s.append(true);  // flag if it is the final DagBlocksPacket or not
  s.appendRaw(packet_bytes, packet_items_count);
  sealAndSend(peer_id, BlocksPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap