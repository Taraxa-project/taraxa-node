#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "votes/rewards_votes.hpp"

namespace taraxa::network::tarcap {

GetDagSyncPacketHandler::GetDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr,
                                                 std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                                 std::shared_ptr<DbStorage> db,
                                                 std::shared_ptr<RewardsVotes> rewards_votes, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)),
      rewards_votes_(std::move(rewards_votes)) {}

void GetDagSyncPacketHandler::process(const PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  std::unordered_set<blk_hash_t> blocks_hashes;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  auto it = packet_data.rlp_.begin();
  const auto mode = static_cast<DagSyncRequestType>((*it++).toInt<unsigned>());

  if (mode == DagSyncRequestType::MissingHashes)
    LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " missing blocks";
  else if (mode == DagSyncRequestType::KnownHashes)
    LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " known blocks";

  for (; it != packet_data.rlp_.end(); ++it) {
    blocks_hashes.emplace(*it);
  }

  const auto &blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      const auto hash = block;
      if (mode == DagSyncRequestType::MissingHashes) {
        if (blocks_hashes.count(hash) == 1) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      } else if (mode == DagSyncRequestType::KnownHashes) {
        if (blocks_hashes.count(hash) == 0) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      }
    }
  }

  // TODO: load all votes from BLOCK_REWARDS_VOTES and send it before sending non-finalize dag blocks, votes from
  //       BLOCK_REWARDS_VOTES are new (not seen before) votes included in to be set dag blocks so full votes have to
  //       be sent so dag blocks can be actually verified

  // TODO: MissingHashes should not be used anymore after Matus's changes...
  // This means that someone requested more hashes that we actually have -> do not send anything
  if (mode == DagSyncRequestType::MissingHashes && dag_blocks.size() != blocks_hashes.size()) {
    LOG(log_nf_) << "Node " << packet_data.from_node_id_ << " requested unknown DAG block";
    return;
  }
  sendBlocks(packet_data.from_node_id_, dag_blocks);
}

void GetDagSyncPacketHandler::sendBlocks(dev::p2p::NodeID const &peer_id,
                                         std::vector<std::shared_ptr<DagBlock>> blocks) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  size_t total_votes_count = 0;
  size_t total_txs_count = 0;
  size_t total_blocks_count = 0;
  std::string blocks_hashes_str{""};

  // TODO: we are not marking anything we sent here as known for peer - is it by purpose ???

  dev::RLPStream s(blocks.size());
  for (const auto &block : blocks) {
    // Each block data consists of block, extra txs, extra votes
    dev::RLPStream block_data_rlp(3);

    // Appends block
    block_data_rlp.appendRaw(block->rlp(true));
    blocks_hashes_str += block->getHash().abridged() + ", ";
    total_blocks_count++;

    // Appends txs
    const auto block_txs_hashes = block->getTrxs();
    block_data_rlp.appendList(block_txs_hashes.size());
    for (const auto &tx_hash : block_txs_hashes) {
      auto tx = trx_mgr_->getTransaction(tx_hash);
      if (!tx) {
        LOG(log_er_) << "SendBlocks canceled. Tx " << tx_hash.abridged() << " is not available.";
        // This can happen on stopping the node because network is not stopped since it does not support restart
        // TODO: better solution needed
        return;
      }

      block_data_rlp.appendRaw(*tx->rlp());
    }
    total_txs_count += block_txs_hashes.size();

    // Appends votes
    const auto &rewards_votes_hashes = block->getRewardsVotes();
    std::unordered_set<vote_hash_t> votes_hashes;
    votes_hashes.reserve(rewards_votes_hashes.size());
    for (const auto &block_reward_vote : rewards_votes_hashes) {
      votes_hashes.insert(block_reward_vote);
    }

    auto res = rewards_votes_->getVotes(std::move(votes_hashes));
    if (!res.first) {
      std::string missing_votes_str{""};
      for (const auto &missing_vote : res.second) {
        missing_votes_str += missing_vote.first.abridged() + ", ";
      }

      LOG(log_er_) << "SendBlocks canceled. Block rewards votes not found: " << missing_votes_str;
      // This can happen on stopping the node because network is not stopped since it does not support restart
      // TODO: better solution needed
      return;
    }
    total_votes_count += rewards_votes_hashes.size();

    auto rewards_votes = std::move(res.second);
    block_data_rlp.appendList(rewards_votes.size());
    for (const auto &vote : rewards_votes) {
      block_data_rlp.appendRaw(vote.second->rlp(true));
    }

    // Append block_data
    s.appendRaw(block_data_rlp.invalidate());
  }

  LOG(log_nf_) << "Send DagSyncPacket with " << total_blocks_count << " blocks, " << total_txs_count << " txs and "
               << total_votes_count << " votes. List of blocks: " << blocks_hashes_str;
  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
