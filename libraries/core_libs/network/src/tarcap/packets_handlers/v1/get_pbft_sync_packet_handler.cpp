#include "network/tarcap/packets_handlers/v1/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "vote/vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v1 {

void GetPbftSyncPacketHandler::sendPbftBlocks(const std::shared_ptr<TaraxaPeer> &peer, PbftPeriod from_period,
                                              size_t blocks_to_transfer, bool pbft_chain_synced) {
  const auto &peer_id = peer->getId();
  LOG(log_tr_) << "sendPbftBlocks: peer want to sync from pbft chain height " << from_period << ", will send at most "
               << blocks_to_transfer << " pbft blocks to " << peer_id;

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

  for (auto block_period = from_period; block_period < from_period + blocks_to_transfer; block_period++) {
    bool last_block = (block_period == from_period + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(block_period);

    if (data.size() == 0) {
      LOG(log_er_) << "DB corrupted. Cannot find period " << block_period << " PBFT block in db";
      assert(false);
    }

    dev::RLPStream s;
    if (pbft_chain_synced && last_block) {
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
    sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, std::move(s));
    if (pbft_chain_synced && last_block) {
      peer->syncing_ = false;
    }
  }
}

}  // namespace taraxa::network::tarcap::v1
