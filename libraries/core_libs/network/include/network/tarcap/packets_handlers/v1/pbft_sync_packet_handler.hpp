#pragma once

#include "network/tarcap/packets_handlers/latest/pbft_sync_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

// V1 packets handlers must be derived from latest packets handlers otherwise netowrk class might not work properly !
class PbftSyncPacketHandler final : public tarcap::PbftSyncPacketHandler {
 public:
  using tarcap::PbftSyncPacketHandler::PbftSyncPacketHandler;

 protected:
  PeriodData decodePeriodData(const dev::RLP& period_data_rlp) const override;
  std::vector<std::shared_ptr<Vote>> decodeVotesBundle(const dev::RLP& votes_bundle_rlp) const override;
};

}  // namespace taraxa::network::tarcap::v1
