#pragma once

#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

// V1 packets handlers must be derived from latest packets handlers otherwise network class might not work properly !
class VotesBundlePacketHandler final : public tarcap::VotesBundlePacketHandler {
 public:
  using tarcap::VotesBundlePacketHandler::VotesBundlePacketHandler;

  void sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer>& peer,
                           std::vector<std::shared_ptr<Vote>>&& votes) override;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  void process(const threadpool::PacketData& packet_data, const std::shared_ptr<tarcap::TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap::v1
