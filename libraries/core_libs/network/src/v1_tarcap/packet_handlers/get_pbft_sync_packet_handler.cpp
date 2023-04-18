#include "network/v1_tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "vote/vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::v1_tarcap {

OmgGetPbftSyncPacketHandler::OmgGetPbftSyncPacketHandler(const FullNodeConfig &conf,
                                                         std::shared_ptr<tarcap::PeersState> peers_state,
                                                         std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                                                         std::shared_ptr<tarcap::PbftSyncingState> pbft_syncing_state,
                                                         std::shared_ptr<PbftChain> pbft_chain,
                                                         std::shared_ptr<VoteManager> vote_mgr,
                                                         std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, "V1_GET_PBFT_SYNC_PH"),
      pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      db_(std::move(db)) {}

void OmgGetPbftSyncPacketHandler::validatePacketRlpFormat(const tarcap::PacketData &packet_data) const {
  if (constexpr size_t required_size = 1; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void OmgGetPbftSyncPacketHandler::process(const tarcap::PacketData &packet_data,
                                          [[maybe_unused]] const std::shared_ptr<tarcap::TaraxaPeer> &peer) {
  LOG(log_tr_) << "Received GetPbftSyncPacket Block";

  const size_t height_to_sync = packet_data.rlp_[0].toInt();
  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  if (height_to_sync > my_chain_size) {
    // Node update peers PBFT chain size in status packet. Should not request syncing period bigger than pbft chain size
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing period start at " << height_to_sync
            << ". That's bigger than own PBFT chain size " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  if (kConf.is_light_node && height_to_sync + kConf.light_node_history <= my_chain_size) {
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing period start at " << height_to_sync
            << ". Light node does not have the data " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  size_t blocks_to_transfer = 0;
  auto pbft_chain_synced = false;
  const auto total_period_datas_size = my_chain_size - height_to_sync + 1;
  if (total_period_datas_size <= kConf.network.sync_level_size) {
    blocks_to_transfer = total_period_datas_size;
    pbft_chain_synced = true;
  } else {
    blocks_to_transfer = kConf.network.sync_level_size;
  }
  LOG(log_tr_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer, pbft_chain_synced);
}

// api for pbft syncing
void OmgGetPbftSyncPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, PbftPeriod from_period,
                                                 size_t blocks_to_transfer, bool pbft_chain_synced) {
  LOG(log_tr_) << "sendPbftBlocks: peer want to sync from pbft chain height " << from_period << ", will send at most "
               << blocks_to_transfer << " pbft blocks to " << peer_id;

  for (auto block_period = from_period; block_period < from_period + blocks_to_transfer; block_period++) {
    bool last_block = (block_period == from_period + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(block_period);

    if (data.size() == 0) {
      LOG(log_er_) << "DB corrupted. Cannot find period " << block_period << " PBFT block in db";
      assert(false);
    }

    // Transform period data rlp from v2 to v1
    auto transformPeriodDataRlpToV1 = [](const dev::bytes &period_data_v2) -> dev::bytes {
      // Create PeriodData old(v1) rlp format
      PeriodData period_data(period_data_v2);

      dev::RLPStream period_data_rlp(PeriodData::kRlpItemCount);
      period_data_rlp.appendRaw(period_data.pbft_blk->rlp(true));
      period_data_rlp.appendList(period_data.previous_block_cert_votes.size());
      for (auto const &v : period_data.previous_block_cert_votes) {
        period_data_rlp.appendRaw(v->rlp(true));
      }
      period_data_rlp.appendList(period_data.dag_blocks.size());
      for (auto const &b : period_data.dag_blocks) {
        period_data_rlp.appendRaw(b.rlp(true));
      }
      period_data_rlp.appendList(period_data.transactions.size());
      for (auto const &t : period_data.transactions) {
        period_data_rlp.appendRaw(t->rlp());
      }

      return period_data_rlp.invalidate();
    };

    dev::RLPStream s;
    if (pbft_chain_synced && last_block && block_period > 1) {
      s.appendList(3);
      s << last_block;
      s.appendRaw(transformPeriodDataRlpToV1(data));

      // Latest finalized block cert votes are saved in db as reward votes for new blocks
      const auto votes = vote_mgr_->getRewardVotes();
      s.appendList(votes.size());
      for (const auto &vote : votes) {
        s.appendRaw(vote->rlp(true));
      }
    } else {
      s.appendList(2);
      s << last_block;
      s.appendRaw(transformPeriodDataRlpToV1(data));
    }
    LOG(log_dg_) << "Sending PbftSyncPacket period " << block_period << " to " << peer_id;
    sealAndSend(peer_id, tarcap::SubprotocolPacketType::PbftSyncPacket, std::move(s));
  }
}

}  // namespace taraxa::network::v1_tarcap
