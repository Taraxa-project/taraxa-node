#include "network/tarcap/packets_handlers/latest/get_pillar_chain_sync_packet_handler.hpp"

#include "pillar_chain/pillar_chain_manager.hpp"
#include "storage/storage.hpp"

namespace taraxa::network::tarcap {

GetPillarChainSyncPacketHandler::GetPillarChainSyncPacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<DbStorage> db,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "GET_PILLAR_CHAIN_SYNC_PH"),
      db_(std::move(db)),
      pillar_chain_manager_(std::move(std::move(pillar_chain_manager))) {}

void GetPillarChainSyncPacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kGetPillarChainSyncPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kGetPillarChainSyncPacketSize);
  }
}

void GetPillarChainSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                              const std::shared_ptr<TaraxaPeer> &peer) {
  const PbftPeriod from_period = packet_data.rlp_[0].toInt();
  const auto to_period = pillar_chain_manager_->getLastFinalizedPillarBlockPeriod();

  const auto &peer_id = peer->getId();
  if (!to_period.has_value()) [[unlikely]] {
    LOG(log_er_) << "Unable to retrieve pillar blocks from period " << from_period
                 << ". Latest finalized pillar block period not available. Peer " << peer_id;
    return;
  }

  for (auto period = from_period; period <= to_period;
       period += kConf.genesis.state.hardforks.ficus_hf.pillar_block_periods) {
    auto pillar_block_data = db_->getPillarBlockData(period);
    if (!pillar_block_data.has_value()) {
      LOG(log_er_) << "Unable to retrieve pillar block data for period " << period << " from db";
      assert(false);
      break;
    }

    dev::RLPStream s;
    s.appendRaw(util::rlp_enc(*pillar_block_data));

    LOG(log_dg_) << "Sending PillarChainSyncPacket for period " << period << " to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PillarChainSyncPacket, std::move(s));
    //    if (pbft_chain_synced && last_block) {
    //      peer->syncing_ = false;
    //    }
  }
}

void GetPillarChainSyncPacketHandler::requestPillarBlocks(PbftPeriod period, const std::shared_ptr<TaraxaPeer> &peer) {
  dev::RLPStream s(1);
  s << period;

  sealAndSend(peer->getId(), SubprotocolPacketType::GetPillarChainSyncPacket, std::move(s));
  LOG(log_dg_) << "Requested pillar blocks for period " << period << " from peer " << peer->getId();
}

}  // namespace taraxa::network::tarcap
