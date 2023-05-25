#pragma once

#include "network/tarcap/packets_handlers/latest/get_next_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

// V1 packets handlers must be derived from latest packets handlers otherwise network class might not work properly !
class GetNextVotesBundlePacketHandler final : public tarcap::GetNextVotesBundlePacketHandler {
 public:
  using tarcap::GetNextVotesBundlePacketHandler::GetNextVotesBundlePacketHandler;

  void sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer>& peer,
                           std::vector<std::shared_ptr<Vote>>&& votes) override;
};

}  // namespace taraxa::network::tarcap::v1
