#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

struct PillarVotesBundlePacket {
  PillarVotesBundlePacket() = default;
  PillarVotesBundlePacket(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket(PillarVotesBundlePacket&&) = default;
  PillarVotesBundlePacket& operator=(const PillarVotesBundlePacket&) = default;
  PillarVotesBundlePacket& operator=(PillarVotesBundlePacket&&) = default;
  PillarVotesBundlePacket(std::vector<std::shared_ptr<PillarVote>>&& pillar_votes)
      : pillar_votes(std::move(pillar_votes)) {}
  PillarVotesBundlePacket(const dev::RLP& packet_rlp) {
    *this = util::rlp_dec<PillarVotesBundlePacket>(packet_rlp);
    if (pillar_votes.size() == 0 || pillar_votes.size() > kMaxPillarVotesInBundleRlp) {
      throw InvalidRlpItemsCountException("PillarVotesBundlePacket", pillar_votes.size(), kMaxPillarVotesInBundleRlp);
    }
  }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  // TODO[2870]: optimize rlp size (use custom class), see encodePillarVotesBundleRlp
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  constexpr static size_t kMaxPillarVotesInBundleRlp{250};

  RLP_FIELDS_DEFINE_INPLACE(pillar_votes)
};

}  // namespace taraxa::network::tarcap
