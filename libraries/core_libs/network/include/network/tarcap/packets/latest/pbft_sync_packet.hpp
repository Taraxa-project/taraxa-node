#pragma once

#include "pbft/period_data.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

struct PbftSyncPacket {
  bool last_block;
  PeriodData period_data;
  std::optional<OptimizedPbftVotesBundle> current_block_cert_votes_bundle;

  RLP_FIELDS_DEFINE_INPLACE(last_block, period_data, current_block_cert_votes_bundle)
};

struct PbftSyncPacketRaw {
  bool last_block;
  dev::bytes period_data;
  std::optional<OptimizedPbftVotesBundle> current_block_cert_votes_bundle;

  void rlp(::taraxa::util::RLPEncoderRef encoding) const {
    encoding.appendList(3);
    encoding.append(last_block);
    encoding.appendRaw(period_data);
    util::rlp(encoding, current_block_cert_votes_bundle);
  }
};

}  // namespace taraxa::network::tarcap
