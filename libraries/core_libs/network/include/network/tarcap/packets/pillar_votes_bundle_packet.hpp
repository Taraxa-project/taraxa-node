#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network {

struct PillarVotesBundlePacket {
  PillarVotesBundlePacket() = default;
  PillarVotesBundlePacket(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket(PillarVotesBundlePacket&&) = default;
  PillarVotesBundlePacket& operator=(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket& operator=(PillarVotesBundlePacket&&) = default;

  //  PillarVotesBundlePacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<PillarVotesBundlePacket>(packet_rlp);
  //  } dev::bytes encode() { return util::rlp_enc(*this); }

  PillarVotesBundlePacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    if (items == 0 || items > kMaxPillarVotesInBundleRlp) {
      throw InvalidRlpItemsCountException("PillarVotesBundlePacket", items, kMaxPillarVotesInBundleRlp);
    }

    for (const auto vote_rlp : packet_rlp) {
      pillar_votes.emplace_back(std::make_shared<PillarVote>(vote_rlp));
    }
  }

  // TODO: will shared_ptr work ?
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  constexpr static size_t kMaxPillarVotesInBundleRlp{250};

  // RLP_FIELDS_DEFINE_INPLACE(pillar_votes)
};

}  // namespace taraxa::network
