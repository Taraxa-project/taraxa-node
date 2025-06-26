#include "network/tarcap/packets_handlers/latest/pbft_blocks_bundle_packet_handler.hpp"

#include "final_chain/final_chain.hpp"
#include "network/tarcap/packets/latest/pbft_blocks_bundle_packet.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

PbftBlocksBundlePacketHandler::PbftBlocksBundlePacketHandler(const FullNodeConfig &conf,
                                                             std::shared_ptr<PeersState> peers_state,
                                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                             std::shared_ptr<PbftManager> pbft_mgr,
                                                             std::shared_ptr<final_chain::FinalChain> final_chain,
                                                             std::shared_ptr<PbftSyncingState> syncing_state,
                                                             const addr_t &node_addr, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "PBFT_BLOCKS_BUNDLE_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      final_chain_(std::move(final_chain)),
      pbft_syncing_state_(syncing_state) {}

void PbftBlocksBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                            const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<PbftBlocksBundlePacket>(packet_data.rlp_);

  if (packet.pbft_blocks.size() > kMaxBlocksInPacket) {
    throw InvalidRlpItemsCountException("PbftBlocksBundlePacket:pbft_blocks", packet.pbft_blocks.size(),
                                        kMaxBlocksInPacket);
  }

  std::unordered_map<PbftPeriod, std::unordered_set<addr_t>> unique_authors;

  if (pbft_syncing_state_->lastSyncingPeer()->getId() != peer->getId()) {
    LOG(log_er_) << "PbftBlocksBundlePacket received from unexpected peer " << peer->getId().abridged()
                 << " last syncing peer " << pbft_syncing_state_->lastSyncingPeer()->getId().abridged();
    // Note: do not throw MaliciousPeerException as in some edge cases node could be already syncing with new peer.
    // In such case we can simply ignore this packet
    return;
  }

  for (const auto &proposed_block : packet.pbft_blocks) {
    const auto proposed_block_period = proposed_block->getPeriod();
    const auto proposed_block_author = proposed_block->getBeneficiary();
    const auto current_pbft_period = pbft_mgr_->getPbftPeriod();

    // Check if proposed block period is relevant compared to the current node period
    if (proposed_block_period < current_pbft_period || proposed_block_period > current_pbft_period + 5) {
      // This should not happen as sender sends PbftBlocksBundlePacket only after he sends last sync packet and
      // PbftBlocksBundlePacket processing is blocked until sync_queue is empty
      LOG(log_er_)
          << "Unable to validate proposed blocks bundle as sync packets were not processed yet. Current chain size "
          << current_pbft_period << ", proposed block period " << proposed_block_period << ", proposed block hash "
          << proposed_block->getBlockHash();

      continue;
    }

    // Check if block author is unique per period
    if (!unique_authors[proposed_block_period].insert(proposed_block_author).second) {
      std::ostringstream err_msg;
      err_msg << "Proposed pbft blocks packet contains non-unique block author " << proposed_block_author;
      throw MaliciousPeerException(err_msg.str());
    }

    // Check if block author is dpos eligible
    if (final_chain_->lastBlockNumber() >= proposed_block_period - 1 &&
        !pbft_mgr_->canParticipateInConsensus(proposed_block_period - 1, proposed_block_author)) {
      std::ostringstream err_msg;
      err_msg << "Proposed pbft blocks packet contains non-eligible block author " << proposed_block_author
              << " for period " << proposed_block_period - 1;
      throw MaliciousPeerException(err_msg.str());
    }

    pbft_mgr_->processProposedBlock(proposed_block);
    LOG(log_dg_) << "Processed received proposed block: " << proposed_block->getBlockHash();
  }
}

}  // namespace taraxa::network::tarcap
