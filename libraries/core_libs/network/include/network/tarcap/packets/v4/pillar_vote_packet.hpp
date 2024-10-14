#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap::v4 {

struct PillarVotePacket {
  PillarVotePacket() = default;
  PillarVotePacket(const PillarVotePacket&) = default;
  PillarVotePacket(PillarVotePacket&&) = default;
  PillarVotePacket& operator=(const PillarVotePacket&) = default;
  PillarVotePacket& operator=(PillarVotePacket&&) = default;

  PillarVotePacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    if (items != PillarVote::kStandardRlpSize) {
      throw InvalidRlpItemsCountException("PillarVotePacket", items, PillarVote::kStandardRlpSize);
    }

    pillar_vote = std::make_shared<PillarVote>(packet_rlp);
  }

  std::shared_ptr<PillarVote> pillar_vote;
};

}  // namespace taraxa::network::tarcap::v4
