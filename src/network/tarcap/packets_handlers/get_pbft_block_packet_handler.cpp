#include "get_pbft_block_packet_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/vote.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "storage/db_storage.hpp"

namespace taraxa::network::tarcap {

GetPbftBlockPacketHandler::GetPbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                     std::shared_ptr<PacketsStats> packets_stats,
                                                     std::shared_ptr<SyncingState> syncing_state,
                                                     std::shared_ptr<PbftChain> pbft_chain,
                                                     std::shared_ptr<DbStorage> db, size_t network_sync_level_size,
                                                     const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      db_(std::move(db)),
      network_sync_level_size_(network_sync_level_size) {}

void GetPbftBlockPacketHandler::process(const dev::RLP &packet_rlp, const PacketData &packet_data,
                                        const std::shared_ptr<dev::p2p::Host> &host __attribute__((unused)),
                                        const std::shared_ptr<TaraxaPeer> &peer __attribute__((unused))) {
  LOG(log_dg_) << "Received GetPbftBlockPacket Block";

  const size_t height_to_sync = packet_rlp[0].toInt();
  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  size_t blocks_to_transfer = 0;
  if (my_chain_size >= height_to_sync) {
    blocks_to_transfer = std::min(network_sync_level_size_, (my_chain_size - (height_to_sync - 1)));
  }
  LOG(log_dg_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  // TODO move this logic somewhere else
  //   Stop syncing if too many sync requests or if the rest of the node is busy
  //   if (blocks_to_transfer > 0) {
  // if (syncing_tp_.num_pending_tasks() >= MAX_SYNCING_NODES) {
  //   LOG(log_er_) << "Node is already serving max syncing nodes, host " << packet_data.from_node_id_.abridged()
  //                << " will be disconnected";
  //   host->disconnect(packet_data.from_node_id_, p2p::UserReason);
  //   break;
  // }
  //   }
  // If blocks_to_transfer is 0, send peer empty PBFT blocks for talking to peer syncing has completed
  //   syncing_tp_.post([host, packet_data.from_node_id_, height_to_sync, blocks_to_transfer, this] {
  //     bool send_blocks = true;
  //     if (blocks_to_transfer > 0) {
  //       uint32_t total_wait_time = 0;
  //       while (tp_.num_pending_tasks() > MAX_NETWORK_QUEUE_TO_DROP_SYNCING) {
  //         uint32_t delay_time = MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS / 10;
  //         thisThreadSleepForMilliSeconds(delay_time);
  //         total_wait_time += delay_time;
  //         if (total_wait_time >= MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS) {
  //           LOG(log_er_) << "Node is busy with " << tp_.num_pending_tasks() << " network packets. Host "
  //                        << packet_data.from_node_id_.abridged() << " will be disconnected";
  //           host->disconnect(packet_data.from_node_id_, p2p::UserReason);
  //           send_blocks = false;
  //           break;
  //         }
  //       }
  //     }
  //     if (send_blocks) sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer);
  //   });
  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer);
}

// api for pbft syncing
void GetPbftBlockPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, size_t height_to_sync,
                                               size_t blocks_to_transfer) {
  LOG(log_dg_) << "In sendPbftBlocks, peer want to sync from pbft chain height " << height_to_sync
               << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peer_id;
  // If blocks_to_transfer is 0, will return empty PBFT blocks
  auto pbft_cert_blks = pbft_chain_->getPbftBlocks(height_to_sync, blocks_to_transfer);
  if (pbft_cert_blks.empty()) {
    sealAndSend(peer_id, PbftBlockPacket, RLPStream(0));
    LOG(log_dg_) << "In sendPbftBlocks, send no pbft blocks to " << peer_id;
    return;
  }
  RLPStream s(pbft_cert_blks.size());
  // Example actual structure:
  // pbft_blk_1 -> [dag_blk_1, dag_blk_2]
  // pbft_blk_2 -> [dag_blk_3]
  // dag_blk_1 -> [trx_1, trx_2, trx_3]
  // dag_blk_2 -> [trx_4, trx_5, trx_6]
  // dag_blk_3 -> [trx_7, trx_8]
  //
  // Represented in the following variables:
  // pbft_blocks = [pbft_cert_blk_1, pbft_cert_blk_2]
  // pbft_blocks_extra = [pbft_blk_1_dag_blk_hashes, pbft_blk_2_dag_blk_hashes]
  // dag_blocks_indexes = [0, 2, 3]
  // dag_blocks = [dag_blk_1, dag_blk_2, dag_blk_3]
  // transactions_indexes = [0, 3, 6, 8]
  // transactions = [trx_1, trx_2, trx_3, trx_4, trx_5, trx_6, trx_7, trx_8]
  // General idea:
  // level_`k`[i] is parent of level_`k+1` elements with ordinals in range from (inclusive) edges_`k`_to_`k+1`[i] to
  // (exclusive) edges_`k`_to_`k+1`[i+1]

  DbStorage::MultiGetQuery db_query(db_);
  auto const &pbft_blocks = pbft_cert_blks;
  for (auto const &b : pbft_blocks) {
    db_query.append(DbStorage::Columns::dag_finalized_blocks, b.pbft_blk->getPivotDagBlockHash(), false);
  }
  auto pbft_blocks_extra = db_query.execute();

  // indexes to dag_blocks vector, based on which we know which dag blocks belong to which pbft block
  // pbft block 0 dag blocks indexes -> <dag_blocks_indexes[0], dag_blocks_indexes[1]> -> <start_idx, end_idx>
  // pbft block 0 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  //
  // pbft block 1 dag blocks indexes -> <dag_blocks_indexes[1], dag_blocks_indexes[2]> -> <start_idx, end_idx>
  // pbft block 1 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  //
  // pbft block 1 dag blocks indexes -> <dag_blocks_indexes[N], dag_blocks_indexes[N+1]> -> <start_idx, end_idx>
  // pbft block 1 dag blocks -> [dag_blocks[start_idx], dag_blocks[start_idx + 1], dag_blocks[end_idx]]
  std::vector<uint> dag_blocks_indexes;
  dag_blocks_indexes.reserve(1 + pbft_blocks.size());
  dag_blocks_indexes.push_back(0);
  for (uint i_0 = 0; i_0 < pbft_blocks.size(); ++i_0) {
    db_query.append(DbStorage::Columns::dag_blocks, dev::RLP(pbft_blocks_extra[i_0]).toVector<h256>());
    dag_blocks_indexes.push_back(db_query.size());
  }
  auto dag_blocks = db_query.execute();

  // indexes to transactions vector, based on which we know which transactions belong to which dag block
  // dag block 0 transactions indexes -> <transactions_indexes[0], transactions_indexes[1]> -> <start_idx, end_idx>
  // dag block 0 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  //
  // dag block 1 transactions indexes -> <transactions_indexes[1], transactions_indexes[2]> -> <start_idx, end_idx>
  // dag block 1 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  //
  // dag block 1 transactions indexes -> <transactions_indexes[N], transactions_indexes[N+1]> -> <start_idx, end_idx>
  // dag block 1 transactions -> [transactions[start_idx], transactions[start_idx + 1], transactions[end_idx]]
  std::vector<uint> transactions_indexes;
  transactions_indexes.reserve(1 + dag_blocks.size());
  transactions_indexes.push_back(0);
  for (auto const &dag_blk_raw : dag_blocks) {
    db_query.append(DbStorage::Columns::transactions, DagBlock::extract_transactions_from_rlp(RLP(dag_blk_raw)));
    transactions_indexes.push_back(db_query.size());
  }
  auto transactions = db_query.execute();

  // Creates final packet out of provided pbft blocks rlp representations
  auto create_packet = [](std::vector<dev::bytes> &&pbft_blocks) -> dev::RLPStream {
    dev::RLPStream packet_rlp;
    packet_rlp.appendList(pbft_blocks.size());
    for (const dev::bytes &block_rlp : pbft_blocks) {
      packet_rlp.appendRaw(std::move(block_rlp));
    }

    return packet_rlp;
  };

  std::vector<dev::bytes> pbft_blocks_rlps;
  uint64_t act_packet_size = 0;

  // Iterate through pbft blocks
  for (uint pbft_block_idx = 0; pbft_block_idx < pbft_blocks.size(); ++pbft_block_idx) {
    RLPStream pbft_block_rlp;
    auto start_1 = dag_blocks_indexes[pbft_block_idx];
    auto end_1 = dag_blocks_indexes[pbft_block_idx + 1];

    std::vector<dev::bytes> dag_blocks_rlps;
    uint64_t dag_blocks_size = 0;

    // Iterate through dag blocks blocks / per pbft block
    for (uint dag_block_idx = start_1; dag_block_idx < end_1; ++dag_block_idx) {
      auto start_2 = transactions_indexes[dag_block_idx];
      auto end_2 = transactions_indexes[dag_block_idx + 1];

      RLPStream dag_rlp;
      dag_rlp.appendList(2);  // item #1 - dag block rlp, item #2 - list of dag block transactions
      dag_rlp.appendRaw(dag_blocks[dag_block_idx]);
      dag_rlp.appendList(end_2 - start_2);

      // Iterate through txs / per dag block
      for (uint trx_idx = start_2; trx_idx < end_2; ++trx_idx) {
        dag_rlp.appendRaw(transactions[trx_idx]);
      }

      // Check if PBFT blocks need to be split and sent in multiple packets so we dont exceed
      // MAX_PACKET_SIZE (15 MB) limit
      if (act_packet_size + dag_blocks_size + dag_rlp.out().size() + pbft_blocks[pbft_block_idx].rlp().size() +
              RLP_OVERHEAD >
          MAX_PACKET_SIZE) {
        LOG(log_dg_) << "Sending partial PbftBlockPacket due tu MAX_PACKET_SIZE limit. " << pbft_block_idx + 1
                     << " blocks out of " << pbft_blocks.size() << " sent.";

        pbft_block_rlp.appendList(1);  // Only list of dag blocks and no pbft block rlp for incomplete packet
        pbft_block_rlp.appendList(dag_blocks_rlps.size());

        for (auto &dag_block : dag_blocks_rlps) pbft_block_rlp.appendRaw(dag_block);

        pbft_blocks_rlps.emplace_back(pbft_block_rlp.invalidate());

        // Send partial packet
        sealAndSend(peer_id, PbftBlockPacket, create_packet(std::move(pbft_blocks_rlps)));

        act_packet_size = 0;
        pbft_blocks_rlps.clear();
        dag_blocks_size = 0;
        dag_blocks_rlps.clear();
      }

      dag_blocks_rlps.push_back(dag_rlp.out());
      dag_blocks_size += dag_rlp.out().size();
    }
    pbft_block_rlp.appendList(2);  // item #1 - list of dag blocks, item #2 pbft block rlp
    pbft_block_rlp.appendList(dag_blocks_rlps.size());

    for (auto &dag_block : dag_blocks_rlps) pbft_block_rlp.appendRaw(dag_block);

    pbft_block_rlp.appendRaw(pbft_blocks[pbft_block_idx].rlp());

    act_packet_size += pbft_block_rlp.out().size();
    pbft_blocks_rlps.emplace_back(pbft_block_rlp.invalidate());
  }

  // Send final packet
  sealAndSend(peer_id, PbftBlockPacket, create_packet(std::move(pbft_blocks_rlps)));
  LOG(log_dg_) << "Sending final PbftBlockPacket to " << peer_id;
}

}  // namespace taraxa::network::tarcap