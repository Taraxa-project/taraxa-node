#include "network/tarcap/packets_handlers/v1/pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap::v1 {

PeriodData PbftSyncPacketHandler::decodePeriodData(const dev::RLP& period_data_rlp) const {
  auto it = period_data_rlp.begin();

  PeriodData period_data;
  period_data.pbft_blk = std::make_shared<PbftBlock>(*it++);
  for (auto const vote_rlp : *it++) {
    period_data.previous_block_cert_votes.emplace_back(std::make_shared<Vote>(vote_rlp));
  }
  for (auto const dag_block_rlp : *it++) {
    period_data.dag_blocks.emplace_back(dag_block_rlp);
  }
  for (auto const trx_rlp : *it) {
    period_data.transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
  }

  return period_data;
}

std::vector<std::shared_ptr<Vote>> PbftSyncPacketHandler::decodeVotesBundle(const dev::RLP& votes_bundle_rlp) const {
  std::vector<std::shared_ptr<Vote>> votes;
  const auto cert_votes_count = votes_bundle_rlp.itemCount();
  votes.reserve(cert_votes_count);

  for (size_t i = 0; i < cert_votes_count; i++) {
    votes.emplace_back(std::make_shared<Vote>(votes_bundle_rlp[i].data().toBytes()));
  }

  return votes;
}

}  // namespace taraxa::network::tarcap::v1
