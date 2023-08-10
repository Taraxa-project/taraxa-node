#pragma once

#include "network/tarcap/packets_handlers/latest/get_pbft_sync_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

// V1 packets handlers must be derived from latest packets handlers otherwise network class might not work properly !
class GetPbftSyncPacketHandler final : public tarcap::GetPbftSyncPacketHandler {
 public:
  using tarcap::GetPbftSyncPacketHandler::GetPbftSyncPacketHandler;

 private:
  void sendPbftBlocks(const std::shared_ptr<TaraxaPeer>& peer, PbftPeriod from_period, size_t blocks_to_transfer,
                      bool pbft_chain_synced) override;
};

}  // namespace taraxa::network::tarcap::v1
