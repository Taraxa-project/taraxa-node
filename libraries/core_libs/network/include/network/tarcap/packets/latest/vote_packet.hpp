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

  std::shared_ptr<PbftVote> vote;
  std::optional<OptionalData> optional_data;

  RLP_FIELDS_DEFINE_INPLACE(vote, optional_data)
};

}  // namespace taraxa::network::tarcap
