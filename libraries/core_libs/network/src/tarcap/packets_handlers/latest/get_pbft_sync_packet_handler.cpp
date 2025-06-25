#include "network/tarcap/packets_handlers/latest/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/packets/latest/pbft_blocks_bundle_packet.hpp"
#include "network/tarcap/packets/latest/pbft_sync_packet.hpp"
#include "network/tarcap/packets_handlers/latest/pbft_blocks_bundle_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

GetPbftSyncPacketHandler::GetPbftSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                                   std::shared_ptr<PbftManager> pbft_mgr,
                                                   std::shared_ptr<PbftChain> pbft_chain,
                                                   std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                                                   const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), logs_prefix + "GET_PBFT_SYNC_PH"),
      pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_mgr_(std::move(pbft_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      db_(std::move(db)) {}

void GetPbftSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                       const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<GetPbftSyncPacket>(packet_data.rlp_);

  logger_->trace("Received GetPbftSyncPacket Block");

  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  if (packet.height_to_sync > my_chain_size) {
    // Node update peers PBFT chain size in status packet. Should not request syncing period bigger than pbft chain size
    std::ostringstream err_msg;
    err_msg << "Peer " << peer->getId() << " request syncing period start at " << packet.height_to_sync
            << ". That's bigger than own PBFT chain size " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  if (kConf.is_light_node && packet.height_to_sync + kConf.light_node_history <= my_chain_size) {
    std::ostringstream err_msg;
    err_msg << "Peer " << peer->getId() << " request syncing period start at " << packet.height_to_sync
            << ". Light node does not have the data " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  size_t blocks_to_transfer = 0;
  auto pbft_chain_synced = false;
  const auto total_period_data_size = my_chain_size - packet.height_to_sync + 1;
  if (total_period_data_size <= kConf.network.sync_level_size) {
    blocks_to_transfer = total_period_data_size;
    pbft_chain_synced = true;
  } else {
    blocks_to_transfer = kConf.network.sync_level_size;
  }
  logger_->trace("Will send {} PBFT blocks to {}", blocks_to_transfer, peer->getId());

  sendPbftBlocks(peer, packet.height_to_sync, blocks_to_transfer, pbft_chain_synced);

  // Send current proposed blocks after the last sync packet
  if (pbft_chain_synced) {
    PbftBlocksBundlePacket proposed_blocks_packet;

    for (auto &&period_proposed_blocks : pbft_mgr_->getProposedBlocks()) {
      for (auto &&proposed_block : period_proposed_blocks.second) {
        proposed_blocks_packet.pbft_blocks.push_back(std::move(proposed_block));

        // Send max kMaxBlocksInPacket(10) blocks in a single packet
        if (proposed_blocks_packet.pbft_blocks.size() == PbftBlocksBundlePacketHandler::kMaxBlocksInPacket) {
          sealAndSend(peer->getId(), SubprotocolPacketType::kPbftBlocksBundlePacket,
                      encodePacketRlp(proposed_blocks_packet));
          proposed_blocks_packet.pbft_blocks.clear();
        }
      }
    }

    if (!proposed_blocks_packet.pbft_blocks.empty()) {
      sealAndSend(peer->getId(), SubprotocolPacketType::kPbftBlocksBundlePacket,
                  encodePacketRlp(proposed_blocks_packet));
    }
  }
}

// api for pbft syncing
void GetPbftSyncPacketHandler::sendPbftBlocks(const std::shared_ptr<TaraxaPeer> &peer, PbftPeriod from_period,
                                              size_t blocks_to_transfer, bool pbft_chain_synced) {
  const auto &peer_id = peer->getId();
  logger_->trace("sendPbftBlocks: peer want to sync from pbft chain height {}, will send at most {} pbft blocks to {}",
                 from_period, blocks_to_transfer, peer_id);

  for (auto block_period = from_period; block_period < from_period + blocks_to_transfer; block_period++) {
    bool last_block = (block_period == from_period + blocks_to_transfer - 1);
    auto period_data = db_->getPeriodDataRaw(block_period);
    if (period_data.empty()) {
      // This can happen when switching from light node to full node setting
      logger_->error("DB corrupted. Cannot find period {} PBFT block in db", block_period);
      return;
    }

    std::shared_ptr<PbftSyncPacketRaw> pbft_sync_packet;

    if (pbft_chain_synced && last_block) {
      // Latest finalized block cert votes are saved in db as reward votes for new blocks
      auto reward_votes = vote_mgr_->getRewardVotes();
      assert(!reward_votes.empty());
      // It is possible that the node pushed another block to the chain in the meantime
      if (reward_votes[0]->getPeriod() == block_period) {
        pbft_sync_packet = std::make_shared<PbftSyncPacketRaw>(last_block, std::move(period_data),
                                                               OptimizedPbftVotesBundle{std::move(reward_votes)});
      } else {
        pbft_sync_packet = std::make_shared<PbftSyncPacketRaw>(last_block, std::move(period_data));
      }
    } else {
      pbft_sync_packet = std::make_shared<PbftSyncPacketRaw>(last_block, std::move(period_data));
    }

    logger_->debug("Sending PbftSyncPacket period {} to {}", block_period, peer_id);
    sealAndSend(peer_id, SubprotocolPacketType::kPbftSyncPacket, encodePacketRlp(pbft_sync_packet));
    if (pbft_chain_synced && last_block) {
      peer->syncing_ = false;
    }
  }
}

}  // namespace taraxa::network::tarcap
