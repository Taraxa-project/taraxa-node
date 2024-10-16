#pragma once

#include "pbft/pbft_block.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa::network::tarcap {

struct VotePacket {
  struct OptionalData {
    std::shared_ptr<PbftBlock> pbft_block;
    uint64_t peer_chain_size;

    RLP_FIELDS_DEFINE_INPLACE(pbft_block, peer_chain_size)
  };

  VotePacket() = default;
  VotePacket(const VotePacket&) = default;
  VotePacket(VotePacket&&) = default;
  VotePacket& operator=(const VotePacket&) = default;
  VotePacket& operator=(VotePacket&&) = default;
  VotePacket(std::shared_ptr<PbftVote> vote, std::optional<OptionalData> optional_data = {})
      : vote(std::move(vote)), optional_data(std::move(optional_data)) {}
  VotePacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<VotePacket>(packet_rlp); };
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  std::shared_ptr<PbftVote> vote;
  std::optional<OptionalData> optional_data;

  RLP_FIELDS_DEFINE_INPLACE(vote, optional_data)
};

}  // namespace taraxa::network::tarcap
