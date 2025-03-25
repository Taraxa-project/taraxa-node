#pragma once

#include "network/tarcap/packets_handlers/latest/get_pbft_sync_packet_handler.hpp"

namespace taraxa::network::tarcap::v4 {

class PbftSyncingState;

class GetPbftSyncPacketHandler : public tarcap::GetPbftSyncPacketHandler {
 public:
  using tarcap::GetPbftSyncPacketHandler::GetPbftSyncPacketHandler;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap::v4
