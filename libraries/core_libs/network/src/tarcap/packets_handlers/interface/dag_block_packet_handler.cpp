#include "network/tarcap/packets_handlers/interface/dag_block_packet_handler.hpp"

namespace taraxa::network::tarcap {

IDagBlockPacketHandler::IDagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                               std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                               const std::string &logs_prefix)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db),
                              logs_prefix) {}

void IDagBlockPacketHandler::onNewBlockVerified(const std::shared_ptr<DagBlock> &block, bool proposed,
                                                const SharedTransactions &trxs) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // gossip is needed
  if (!proposed && pbft_syncing_state_->isDeepPbftSyncing()) {
    return;
  }

  const auto &block_hash = block->getHash();
  logger_->trace("Verified dag block {}", block_hash.toString());

  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isDagBlockKnown(block_hash) && !peer.second->syncing_) {
      peers_to_send.push_back(peer.first);
    }
  }

  // Sending it in same order favours some peers over others, always start with a different position
  const auto peers_to_send_count = peers_to_send.size();
  if (peers_to_send_count == 0) {
    return;
  }

  std::string peer_and_transactions_to_log;
  uint32_t start_with = rand() % peers_to_send_count;
  for (uint32_t i = 0; i < peers_to_send_count; i++) {
    auto peer_id = peers_to_send[(start_with + i) % peers_to_send_count];
    auto peer = peers_state_->getPeer(peer_id);
    if (!peer || peer->syncing_) {
      continue;
    }

    peer_and_transactions_to_log += " Peer: " + peer->getId().abridged() + " Trxs: ";

    SharedTransactions transactions_to_send;
    for (const auto &trx : trxs) {
      assert(trx != nullptr);
      const auto trx_hash = trx->getHash();
      if (peer->isTransactionKnown(trx_hash)) {
        continue;
      }

      transactions_to_send.push_back(trx);
      peer_and_transactions_to_log += trx_hash.abridged();
    }

    sendBlockWithTransactions(peer, block, std::move(transactions_to_send));
  }

  logger_->debug("Send DagBlock {} to peers: {}", block->getHash(), peer_and_transactions_to_log);
  logger_->trace("Sent block to {} peers", peers_to_send.size());
}

void IDagBlockPacketHandler::requestDagBlocks(std::shared_ptr<TaraxaPeer> peer) { requestPendingDagBlocks(peer); }

}  // namespace taraxa::network::tarcap
