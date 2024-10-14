#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "pbft/pbft_block.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa::network::tarcap::v4 {

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

  static constexpr size_t kVotePacketSize{1};
  static constexpr size_t kExtendedVotePacketSize{3};
};

}  // namespace taraxa::network::tarcap::v4
