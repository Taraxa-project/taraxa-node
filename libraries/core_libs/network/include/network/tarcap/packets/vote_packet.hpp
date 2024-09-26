#pragma once

#include "pbft/pbft_block.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa::network {

// TODO: create new version of this packet without manual parsing
struct VotePacket {
  VotePacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    // Vote packet can contain either just a vote or vote + block + peer_chain_size
    if (items != kVotePacketSize && items != kExtendedVotePacketSize) {
      throw InvalidRlpItemsCountException("VotePacket", items, kExtendedVotePacketSize);
    }

    vote = std::make_shared<PbftVote>(packet_rlp[0]);
    if (const size_t item_count = packet_rlp.itemCount(); item_count == kExtendedVotePacketSize) {
      try {
        pbft_block = std::make_shared<PbftBlock>(packet_rlp[1]);
      } catch (const std::exception& e) {
        throw MaliciousPeerException(e.what());
      }
      peer_chain_size = packet_rlp[2].toInt();
    }
  };

  std::shared_ptr<PbftVote> vote;
  std::shared_ptr<PbftBlock> pbft_block;
  std::optional<uint64_t> peer_chain_size;

  const size_t kVotePacketSize{1};
  const size_t kExtendedVotePacketSize{3};

  // RLP_FIELDS_DEFINE_INPLACE(transactions, dag_block)
};

}  // namespace taraxa::network
