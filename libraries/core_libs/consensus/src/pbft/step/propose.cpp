#include "pbft/step/propose.hpp"

#include <cassert>

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::step {

void Propose::run() {
  // Value Proposal
  auto round = round_->getId();
  if (round == 1) {
    // Round 1 cannot propose block. Everyone has to next vote kNullBlockHash in round 1 to make consensus go to next
    // round
    finish_();
    return;
  }

  LOG(log_tr_) << "PBFT value propose step in round " << round;
  // Round greater than 1

  if (round_->previous_round_next_voted_null_block_hash_) {
    LOG(log_nf_) << "Previous round " << round - 1 << " next voted block is kNullBlockHash";
  } else if (round_->previous_round_next_voted_value_) {
    LOG(log_nf_) << "Previous round " << round - 1 << " next voted block is "
                 << round_->previous_round_next_voted_value_;
  } else {
    LOG(log_er_) << "Previous round " << round - 1 << " doesn't have enough next votes";
    assert(false);
  }

  if (giveUpNextVotedBlock_()) {
    // PBFT block only be able to propose once in each period
    if (!proposed_block_hash_) {
      proposed_block_hash_ = proposePbftBlock_();
    }
    if (proposed_block_hash_) {
      node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound, proposed_block_hash_);
      round_->own_starting_value_for_round_ = proposed_block_hash_;

      auto place_votes = placeVote_(round_->own_starting_value_for_round_, propose_vote_type, round);
      if (place_votes) {
        LOG(log_nf_) << "Proposing " << place_votes << " own starting value " << round_->own_starting_value_for_round_
                     << " for round " << round;
      }
    }
  } else if (round_->previous_round_next_voted_value_) {
    node_->db_->savePbftMgrVotedValue(PbftMgrVotedValue::OwnStartingValueInRound,
                                      round_->previous_round_next_voted_value_);
    round_->own_starting_value_for_round_ = round_->previous_round_next_voted_value_;

    auto pbft_block = node_->pbft_chain_->getUnverifiedPbftBlock(round_->own_starting_value_for_round_);
    if (!pbft_block) {
      LOG(log_dg_) << "Can't get proposal block " << round_->own_starting_value_for_round_ << " in unverified queue";
      pbft_block = node_->db_->getPbftCertVotedBlock(round_->own_starting_value_for_round_);
      if (!pbft_block) {
        LOG(log_dg_) << "Can't get proposal block " << round_->own_starting_value_for_round_ << " in database";
      }
    }
    if (pbft_block) {
      // place vote
      auto place_votes = placeVote_(round_->own_starting_value_for_round_, propose_vote_type, round);
      if (place_votes) {
        LOG(log_nf_) << "Rebroadcast next voted block " << round_->own_starting_value_for_round_ << ", and propose"
                     << place_votes << " votes "
                     << " from previous round. In round " << round;
        // broadcast pbft block
        if (auto net = node_->network_.lock()) {
          net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
        }
      }
    }
  }
  finish_();
}

void Propose::finish_() {
  Step::finish_();
  round_->sleepUntil(2 * std::chrono::milliseconds(round_->getLambda()));
}

std::optional<blk_hash_t> findClosestAnchor(const std::vector<blk_hash_t> &ghost,
                                            const std::vector<blk_hash_t> &dag_order, uint32_t included) {
  for (uint32_t i = included; i > 0; i--) {
    if (std::count(ghost.begin(), ghost.end(), dag_order[i - 1])) {
      return dag_order[i - 1];
    }
  }
  return ghost[1];
}

blk_hash_t Propose::proposePbftBlock_() {
  auto pm = node_->pbft_manager_.lock();
  if (!pm) {
    return {};
  }
  auto round = round_->getId();
  VrfPbftSortition vrf_sortition(node_->vrf_sk_, {propose_vote_type, round, 1});
  if (pm->weighted_votes_count_ == 0 ||
      !vrf_sortition.calculateWeight(pm->getDposWeightedVotesCount(), pm->getDposTotalVotesCount(),
                                     pm->getThreshold(propose_vote_type), dev::toPublic(node_->node_sk_))) {
    return kNullBlockHash;
  }

  // LOG(log_dg_) << "Into propose PBFT block";
  blk_hash_t last_period_dag_anchor_block_hash;
  auto last_pbft_block_hash = node_->pbft_chain_->getLastPbftBlockHash();
  if (last_pbft_block_hash) {
    auto prev_block_hash = last_pbft_block_hash;
    auto prev_pbft_block = node_->pbft_chain_->getPbftBlockInChain(prev_block_hash);
    last_period_dag_anchor_block_hash = prev_pbft_block.getPivotDagBlockHash();
    while (!last_period_dag_anchor_block_hash) {
      // The anchor is NULL BLOCK HASH
      prev_block_hash = prev_pbft_block.getPrevBlockHash();
      if (!prev_block_hash) {
        // The genesis PBFT block head
        last_period_dag_anchor_block_hash = pm->dag_genesis_block_hash_;
        break;
      }
      prev_pbft_block = node_->pbft_chain_->getPbftBlockInChain(prev_block_hash);
      last_period_dag_anchor_block_hash = prev_pbft_block.getPivotDagBlockHash();
    }
  } else {
    // First PBFT block
    last_period_dag_anchor_block_hash = pm->dag_genesis_block_hash_;
  }

  std::vector<blk_hash_t> ghost;
  node_->dag_mgr_->getGhostPath(last_period_dag_anchor_block_hash, ghost);
  LOG(log_dg_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at least include the last period dag anchor block
  if (ghost.empty()) {
    LOG(log_dg_) << "GHOST is empty. No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
  }

  blk_hash_t dag_block_hash;
  if (ghost.size() <= node_->pbft_config_.dag_blocks_size) {
    // Move back  config_.ghost_path_move_back DAG blocks for DAG sycning
    auto ghost_index = (ghost.size() < node_->pbft_config_.ghost_path_move_back + 1)
                           ? 0
                           : (ghost.size() - 1 - node_->pbft_config_.ghost_path_move_back);
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = ghost[ghost_index];
  } else {
    dag_block_hash = ghost[node_->pbft_config_.dag_blocks_size - 1];
  }

  if (dag_block_hash == pm->dag_genesis_block_hash_) {
    LOG(log_dg_) << "No new DAG blocks generated. DAG only has genesis " << dag_block_hash
                 << " PBFT propose NULL BLOCK HASH anchor";
    return generatePbftBlock(last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
  }

  // Compare with last dag block hash. If they are same, which means no new dag blocks generated since last round. In
  // that case PBFT proposer should propose NULL BLOCK HASH anchor as their value
  if (dag_block_hash == last_period_dag_anchor_block_hash) {
    LOG(log_dg_) << "Last period DAG anchor block hash " << dag_block_hash
                 << " No new DAG blocks generated, PBFT propose NULL BLOCK HASH anchor";
    LOG(log_dg_) << "Ghost: " << ghost;
    return generatePbftBlock(last_pbft_block_hash, kNullBlockHash, kNullBlockHash);
  }

  // get DAG block and transaction order
  const auto propose_period = node_->pbft_chain_->getPbftChainSize() + 1;
  auto dag_block_order = node_->dag_mgr_->getDagBlockOrder(dag_block_hash, propose_period);

  if (dag_block_order.empty()) {
    LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlockOrder failed in propose";
    assert(false);
  }

  std::unordered_set<trx_hash_t> trx_hashes_set;
  std::vector<trx_hash_t> trx_hashes;
  u256 total_weight = 0;
  uint32_t dag_blocks_included = 0;
  for (auto const &blk_hash : dag_block_order) {
    auto dag_blk = node_->dag_blk_mgr_->getDagBlock(blk_hash);
    if (!dag_blk) {
      LOG(log_er_) << "DAG anchor block hash " << dag_block_hash << " getDagBlock failed in propose for block "
                   << blk_hash;
      assert(false);
    }
    u256 dag_block_weight = 0;
    const auto &estimations = dag_blk->getTrxsGasEstimations();

    int32_t i = 0;
    for (const auto &trx_hash : dag_blk->getTrxs()) {
      if (trx_hashes_set.emplace(trx_hash).second) {
        trx_hashes.emplace_back(trx_hash);
        dag_block_weight += estimations[i];
      }
      i++;
    }
    if (total_weight + dag_block_weight > node_->pbft_config_.gas_limit) {
      // we need to form new list of transactions after clipping if block is overweighted
      trx_hashes.clear();
      break;
    }
    total_weight += dag_block_weight;
    dag_blocks_included++;
  }

  if (dag_blocks_included != dag_block_order.size()) {
    auto closest_anchor = findClosestAnchor(ghost, dag_block_order, dag_blocks_included);
    if (!closest_anchor) {
      LOG(log_er_) << "Can't find closest anchor after block clipping. Ghost: " << ghost << ". Clipped  block_order: "
                   << vec_blk_t(dag_block_order.begin(), dag_block_order.begin() + dag_blocks_included);
      assert(false);
    }

    dag_block_hash = closest_anchor.value();
    dag_block_order = node_->dag_mgr_->getDagBlockOrder(dag_block_hash, propose_period);
  }
  if (trx_hashes.empty()) {
    std::unordered_set<trx_hash_t> trx_set;
    std::vector<trx_hash_t> transactions_to_query;
    for (auto const &dag_blk_hash : dag_block_order) {
      auto dag_block = node_->dag_blk_mgr_->getDagBlock(dag_blk_hash);
      assert(dag_block);
      for (auto const &trx_hash : dag_block->getTrxs()) {
        if (trx_set.insert(trx_hash).second) {
          trx_hashes.emplace_back(trx_hash);
        }
      }
    }
  }
  std::vector<trx_hash_t> non_finalized_transactions;
  auto trx_finalized = node_->db_->transactionsFinalized(trx_hashes);
  for (uint32_t i = 0; i < trx_finalized.size(); i++) {
    if (!trx_finalized[i]) {
      non_finalized_transactions.emplace_back(trx_hashes[i]);
    }
  }

  const auto transactions = node_->trx_mgr_->getNonfinalizedTrx(non_finalized_transactions, true /*sorted*/);
  non_finalized_transactions.clear();
  std::transform(transactions.begin(), transactions.end(), std::back_inserter(non_finalized_transactions),
                 [](const auto &t) { return t->getHash(); });

  auto order_hash = pbft::calculateOrderHash(dag_block_order, non_finalized_transactions);
  auto pbft_block_hash = generatePbftBlock(last_pbft_block_hash, dag_block_hash, order_hash);
  LOG(log_nf_) << "Proposed Pbft block: " << pbft_block_hash << ". Order hash:" << order_hash
               << ". DAG order for proposed block" << dag_block_order << ". Transaction order for proposed block"
               << non_finalized_transactions;

  return pbft_block_hash;
}

blk_hash_t Propose::generatePbftBlock(const blk_hash_t &prev_blk_hash, const blk_hash_t &anchor_hash,
                                      const blk_hash_t &order_hash) {
  const auto propose_period = node_->pbft_chain_->getPbftChainSize() + 1;
  const auto reward_votes = node_->vote_mgr_->getRewardVotes();
  std::vector<vote_hash_t> reward_votes_hashes;
  std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                 [](const auto &v) { return v->getHash(); });
  const auto pbft_block =
      std::make_shared<PbftBlock>(prev_blk_hash, anchor_hash, order_hash, propose_period, node_->node_addr_,
                                  node_->node_sk_, std::move(reward_votes_hashes));

  // push pbft block
  node_->pbft_chain_->pushUnverifiedPbftBlock(pbft_block);

  // broadcast pbft block
  if (auto net = node_->network_.lock()) {
    net->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->onNewPbftBlock(pbft_block);
  }

  LOG(log_dg_) << node_->node_addr_ << " propose PBFT block succussful! in round: " << round_->getId()
               << " in step: " << id_ << " PBFT block: " << pbft_block->getBlockHash();

  return pbft_block->getBlockHash();
}

}  // namespace taraxa::step