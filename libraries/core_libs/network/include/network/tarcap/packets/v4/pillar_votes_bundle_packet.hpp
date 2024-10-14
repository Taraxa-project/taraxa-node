#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap::v4 {

struct PillarVotesBundlePacket {
  PillarVotesBundlePacket() = default;
  PillarVotesBundlePacket(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket(PillarVotesBundlePacket&&) = default;
  PillarVotesBundlePacket& operator=(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket& operator=(PillarVotesBundlePacket&&) = default;

  PillarVotesBundlePacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    if (items == 0 || items > kMaxPillarVotesInBundleRlp) {
      throw InvalidRlpItemsCountException("PillarVotesBundlePacket", items, kMaxPillarVotesInBundleRlp);
    }

    for (const auto vote_rlp : packet_rlp) {
      pillar_votes.emplace_back(std::make_shared<PillarVote>(vote_rlp));
    }
  }

  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  constexpr static size_t kMaxPillarVotesInBundleRlp{250};
};

}  // namespace taraxa::network::tarcap::v4
