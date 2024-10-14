#pragma once

#include "pbft/pbft_block.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa::network::tarcap {

struct VotePacket {
  VotePacket() = default;
  VotePacket(const VotePacket&) = default;
  VotePacket(VotePacket&&) = default;
  VotePacket& operator=(const VotePacket&) = default;
  VotePacket& operator=(VotePacket&&) = default;
  VotePacket(std::shared_ptr<PbftVote> vote, std::shared_ptr<PbftBlock> pbft_block = {},
             std::optional<uint64_t> peer_chain_size = {})
      : vote(std::move(vote)), pbft_block(std::move(pbft_block)), peer_chain_size(std::move(peer_chain_size)) {}
  VotePacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<VotePacket>(packet_rlp); };
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  std::shared_ptr<PbftVote> vote;
  // TODO: Should it be also optional ?
  std::shared_ptr<PbftBlock> pbft_block;
  std::optional<uint64_t> peer_chain_size;

  RLP_FIELDS_DEFINE_INPLACE(vote, pbft_block, peer_chain_size)
};

}  // namespace taraxa::network::tarcap
