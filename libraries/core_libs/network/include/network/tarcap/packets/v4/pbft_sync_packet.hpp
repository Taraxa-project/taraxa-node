#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "pbft/period_data.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap::v4 {

struct PbftSyncPacket {
  PbftSyncPacket(const dev::RLP& packet_rlp) {
    if (packet_rlp.itemCount() != kStandardPacketSize && packet_rlp.itemCount() != kChainSyncedPacketSize) {
      throw InvalidRlpItemsCountException("PbftSyncPacket", packet_rlp.itemCount(), kStandardPacketSize);
    }

    // PeriodData rlp parsing cannot be done through util::rlp_tuple, which automatically checks the rlp size so it is
    // checked here manually
    if (packet_rlp[1].itemCount() != PeriodData::kBaseRlpItemCount &&
        packet_rlp[1].itemCount() != PeriodData::kExtendedRlpItemCount) {
      throw InvalidRlpItemsCountException("PbftSyncPacket:PeriodData", packet_rlp[1].itemCount(),
                                          PeriodData::kBaseRlpItemCount);
    }

    // last_block is the flag to indicate this is the last block in each syncing round, doesn't mean PBFT chain has
    // synced
    last_block = packet_rlp[0].toInt<bool>();
    try {
      period_data = PeriodData(packet_rlp[1]);
    } catch (const std::runtime_error& e) {
      throw MaliciousPeerException("Unable to parse PeriodData: " + std::string(e.what()));
    }

    // pbft_chain_synced is the flag to indicate own PBFT chain has synced with the peer's PBFT chain
    if (packet_rlp.itemCount() == kChainSyncedPacketSize) {
      current_block_cert_votes = decodePbftVotesBundleRlp(packet_rlp[2]);
    }
  };

  bool last_block;
  PeriodData period_data;
  std::vector<std::shared_ptr<PbftVote>> current_block_cert_votes;

  const size_t kStandardPacketSize = 2;
  const size_t kChainSyncedPacketSize = 3;
};

}  // namespace taraxa::network::tarcap::v4
