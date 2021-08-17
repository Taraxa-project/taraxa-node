#pragma once

#include "dag/dag_block.hpp"
#include "get_blocks_request_type.hpp"
#include "packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DagManager;
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

/**
 * @brief SyncingHandler is not a classic handler that would processing any incoming packets. It is a set of common
 *        functions that are used in packet handlers that need to interact with syncing process in some way
 */
class SyncingHandler : public PacketHandler {
 public:
  SyncingHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                 std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                 const addr_t &node_addr = {});

  /**
   * @note This method is not intended to be used for SyncingHandler
   */
  void process(const dev::RLP &packet_rlp, const PacketData &packet_data, const std::shared_ptr<dev::p2p::Host> &host,
               const std::shared_ptr<TaraxaPeer> &peer) override;

  /**
   * @brief Restart syncing
   *
   * @param shared_state
   * @param force
   */
  void restartSyncingPbft(bool force);

  void syncPeerPbft(unsigned long height_to_sync);
  void requestBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                     GetBlocksPacketRequestType mode);
  void syncPbftNextVotes(uint64_t pbft_round, size_t pbft_previous_round_next_votes_size);

  std::pair<bool, std::vector<blk_hash_t>> checkDagBlockValidation(DagBlock const &block) const;

 private:
  void requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync);
  void requestPendingDagBlocks();
  void requestPbftNextVotes(dev::p2p::NodeID const &peerID, uint64_t pbft_round,
                            size_t pbft_previous_round_next_votes_size);

 private:
  std::shared_ptr<SyncingState> syncing_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DagBlockManager> dag_blk_mgr_{nullptr};
};

}  // namespace taraxa::network::tarcap
