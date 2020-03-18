/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-10
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "pbft_manager.hpp"

#include <libdevcore/SHA3.h>

#include <chrono>
#include <string>

#include "dag.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "sortition.hpp"
#include "util.hpp"

namespace taraxa {

PbftManager::PbftManager(std::string const &genesis) : dag_genesis_(genesis) {}
PbftManager::PbftManager(std::vector<uint> const &params,
                         std::string const &genesis)
    // TODO: for debug, need remove later
    : LAMBDA_ms_MIN(params[0]),
      COMMITTEE_SIZE(params[1]),
      VALID_SORTITION_COINS(params[2]),
      DAG_BLOCKS_SIZE(params[3]),
      GHOST_PATH_MOVE_BACK(params[4]),
      SKIP_PERIODS(params[5]),
      RUN_COUNT_VOTES(params[6]),
      dag_genesis_(genesis) {}

void PbftManager::setFullNode(
    shared_ptr<taraxa::FullNode> full_node,
    shared_ptr<ReplayProtectionService> replay_protection_service) {
  pbft_state_machine_ = std::make_shared<PbftStateMachine>(this);
  node_ = full_node;
  vote_mgr_ = full_node->getVoteManager();
  pbft_chain_ = full_node->getPbftChain();
  capability_ = full_node->getNetwork()->getTaraxaCapability();
  replay_protection_service_ = replay_protection_service;
  db_ = full_node->getDB();
}

void PbftManager::start() {
  if (bool b = true; !stop_.compare_exchange_strong(b, !b)) {
    return;
  }
  auto full_node = node_.lock();
  std::vector<std::string> ghost;
  full_node->getGhostPath(dag_genesis_, ghost);
  while (ghost.empty()) {
    LOG(log_deb_)
        << "GHOST is empty. DAG initialization has not done. Sleep 100ms";
    thisThreadSleepForMilliSeconds(100);
  }
  LOG(log_deb_) << "PBFT start at GHOST size " << ghost.size()
                << ", the last of DAG blocks is " << ghost.back();

  if (!db_->sortitionAccountInDb(std::string("sortition_accounts_size"))) {
    // New node
    // Initialize master boot node account balance
    addr_t master_boot_node_address = full_node->getMasterBootNodeAddress();
    std::pair<val_t, bool> master_boot_node_account_balance =
        full_node->getBalance(master_boot_node_address);
    if (!master_boot_node_account_balance.second) {
      LOG(log_err_) << "Failed initial master boot node account balance."
                    << " Master boot node balance is not exist.";
    } else if (master_boot_node_account_balance.first != TARAXA_COINS_DECIMAL) {
      LOG(log_inf_)
          << "Initial master boot node account balance. Current balance "
          << master_boot_node_account_balance.first;
    }
    PbftSortitionAccount master_boot_node(
        master_boot_node_address, master_boot_node_account_balance.first, 0,
        new_change);
    sortition_account_balance_table_tmp[master_boot_node_address] =
        master_boot_node;
    auto batch = db_->createWriteBatch();
    updateSortitionAccountsDB_(batch);
    db_->commitWriteBatch(batch);
    updateSortitionAccountsTable_();
  } else {
    // Full node join back
    if (!sortition_account_balance_table.empty()) {
      LOG(log_err_) << "PBFT sortition accounts table should be empty";
      assert(false);
    }
    size_t sortition_accounts_size_db = 0;
    db_->forEachSortitionAccount([&](auto const &key, auto const &value) {
      if (key.ToString() == "sortition_accounts_size") {
        std::stringstream sstream(value.ToString());
        sstream >> sortition_accounts_size_db;
        return true;
      }
      PbftSortitionAccount account(value.ToString());
      sortition_account_balance_table_tmp[account.address] = account;
      return true;
    });
    updateSortitionAccountsTable_();
    assert(sortition_accounts_size_db == valid_sortition_accounts_size_);
  }

  if (RUN_COUNT_VOTES) {
    stop_monitor_ = false;
    monitor_votes_ = std::make_shared<std::thread>([this]() { countVotes_(); });
    LOG(log_inf_test_) << "PBFT monitor vote logs initiated";
  }
  pbft_state_machine_->start();
}

void PbftManager::stop() {
  if (bool b = false; !stop_.compare_exchange_strong(b, !b)) {
    return;
  }
  pbft_state_machine_->stop();
  if (RUN_COUNT_VOTES) {
    stop_monitor_ = true;
    monitor_votes_->join();
    LOG(log_inf_test_) << "PBFT monitor vote logs terminated";
  }
  replay_protection_service_ = nullptr;
}

std::pair<bool, uint64_t> PbftManager::getDagBlockPeriod(
    blk_hash_t const &hash) {
  std::pair<bool, uint64_t> res;
  auto value = db_->getDagBlockPeriod(hash);
  if (value == nullptr) {
    res.first = false;
  } else {
    res.first = true;
    res.second = *value;
  }
  return res;
}

std::string PbftManager::getScheduleBlockByPeriod(uint64_t period) {
  auto value = db_->getPeriodScheduleBlock(period);
  if (value) {
    blk_hash_t pbft_block_hash = *value;
    return pbft_chain_->getPbftBlockInChain(pbft_block_hash).getJsonStr();
  }
  return "";
}

bool PbftManager::shouldSpeak(PbftVoteTypes type, uint64_t round, size_t step) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable";
    return false;
  }
  addr_t account_address = full_node->getAddress();
  if (sortition_account_balance_table.find(account_address) ==
      sortition_account_balance_table.end()) {
    LOG(log_tra_) << "Cannot find account " << account_address
                  << " in sortition table. Don't have enough coins to vote";
    return false;
  }
  // only active players are able to vote
  if (!is_active_player_) {
    LOG(log_tra_)
        << "Account " << account_address << " last period seen at "
        << sortition_account_balance_table[account_address].last_period_seen
        << ", as a non-active player";
    return false;
  }

  // compute sortition
  VrfPbftMsg msg(pbft_chain_last_block_hash_, type, round, step);
  VrfPbftSortition vrf_sortition(full_node->getVrfSecretKey(), msg);
  if (!vrf_sortition.canSpeak(sortition_threshold_,
                              valid_sortition_accounts_size_)) {
    LOG(log_tra_) << "Don't get sortition";
    return false;
  }
  return true;
}

/* There is a quorum of next-votes and set determine that round p should be the
 * current round...
 */
uint64_t PbftManager::roundDeterminedFromVotes_(std::vector<Vote> votes) {
  // <<vote_round, vote_step>, count>, <round, step> store in reverse order
  std::map<std::pair<uint64_t, size_t>, size_t,
           std::greater<std::pair<uint64_t, size_t>>>
      next_votes_tally_by_round_step;

  for (Vote &v : votes) {
    if (v.getType() != next_vote_type) {
      continue;
    }

    std::pair<uint64_t, size_t> round_step =
        std::make_pair(v.getRound(), v.getStep());
    if (round_step.first >= round_) {
      if (next_votes_tally_by_round_step.find(round_step) !=
          next_votes_tally_by_round_step.end()) {
        next_votes_tally_by_round_step[round_step] += 1;
      } else {
        next_votes_tally_by_round_step[round_step] = 1;
      }
    }
  }

  for (auto &rs_votes : next_votes_tally_by_round_step) {
    if (rs_votes.second >= TWO_T_PLUS_ONE) {
      std::vector<Vote> next_votes_for_round_step =
          getVotesOfTypeFromVotesForRoundAndStep_(
              next_vote_type, votes, rs_votes.first.first,
              rs_votes.first.second, std::make_pair(NULL_BLOCK_HASH, false));
      if (blockWithEnoughVotes_(next_votes_for_round_step).second) {
        LOG(log_deb_) << "Found sufficient next votes in round "
                      << rs_votes.first.first << ", step "
                      << rs_votes.first.second;
        return rs_votes.first.first + 1;
      }
    }
  }
  return round_;
}

// Assumption is that all votes are in the same round and of same type...
std::pair<blk_hash_t, bool> PbftManager::blockWithEnoughVotes_(
    std::vector<Vote> const &votes) const {
  bool is_first_block = true;
  PbftVoteTypes vote_type;
  uint64_t vote_round;
  blk_hash_t blockhash;
  // <block_hash, count>, store in reverse order
  std::map<blk_hash_t, size_t, std::greater<blk_hash_t>> tally_by_blockhash;

  for (Vote const &v : votes) {
    if (is_first_block) {
      vote_type = v.getType();
      vote_round = v.getRound();
      is_first_block = false;
    } else {
      bool vote_type_and_round_is_consistent =
          (vote_type == v.getType() && vote_round == v.getRound());
      if (!vote_type_and_round_is_consistent) {
        LOG(log_err_) << "Vote types and rounds were not internally"
                         " consistent!";
        assert(false);
        // return std::make_pair(NULL_BLOCK_HASH, false);
      }
    }

    blockhash = v.getBlockHash();
    if (tally_by_blockhash.find(blockhash) != tally_by_blockhash.end()) {
      tally_by_blockhash[blockhash] += 1;
    } else {
      tally_by_blockhash[blockhash] = 1;
    }

    for (auto const &blockhash_pair : tally_by_blockhash) {
      if (blockhash_pair.second >= TWO_T_PLUS_ONE) {
        LOG(log_deb_) << "find block hash " << blockhash_pair.first
                      << " vote type " << vote_type << " in round "
                      << vote_round << " has " << blockhash_pair.second
                      << " votes";
        return std::make_pair(blockhash_pair.first, true);
      } else {
        LOG(log_tra_) << "Don't have enough votes. block hash "
                      << blockhash_pair.first << " vote type " << vote_type
                      << " in round " << vote_round << " has "
                      << blockhash_pair.second << " votes"
                      << " (2TP1 = " << TWO_T_PLUS_ONE << ")";
      }
    }
  }

  return std::make_pair(NULL_BLOCK_HASH, false);
}

std::map<size_t, std::vector<Vote>, std::greater<size_t>>
PbftManager::getVotesOfTypeFromVotesForRoundByStep_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
    std::pair<blk_hash_t, bool> blockhash) {
  //<vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
      requested_votes_by_step;

  for (Vote &v : votes) {
    if (v.getType() == vote_type && v.getRound() == round &&
        (blockhash.second == false || blockhash.first == v.getBlockHash())) {
      size_t step = v.getStep();
      if (requested_votes_by_step.find(step) == requested_votes_by_step.end()) {
        requested_votes_by_step[step] = {v};
      } else {
        requested_votes_by_step[step].emplace_back(v);
      }
    }
  }

  return requested_votes_by_step;
}

std::vector<Vote> PbftManager::getVotesOfTypeFromVotesForRoundAndStep_(
    PbftVoteTypes vote_type, std::vector<Vote> &votes, uint64_t round,
    size_t step, std::pair<blk_hash_t, bool> blockhash) {
  std::vector<Vote> votes_of_requested_type;
  std::copy_if(votes.begin(), votes.end(),
               std::back_inserter(votes_of_requested_type),
               [vote_type, round, step, blockhash](Vote const &v) {
                 return (v.getType() == vote_type && v.getRound() == round &&
                         v.getStep() == step &&
                         (blockhash.second == false ||
                          blockhash.first == v.getBlockHash()));
               });

  return votes_of_requested_type;
}

std::pair<blk_hash_t, bool> PbftManager::nextVotedBlockForRoundAndStep_(
    std::vector<Vote> &votes, uint64_t round) {
  //<vote_step, votes> reverse order
  std::map<size_t, std::vector<Vote>, std::greater<size_t>>
      next_votes_in_round_by_step = getVotesOfTypeFromVotesForRoundByStep_(
          next_vote_type, votes, round, std::make_pair(NULL_BLOCK_HASH, false));

  std::pair<blk_hash_t, bool> next_vote_block_hash =
      std::make_pair(NULL_BLOCK_HASH, false);
  for (auto &sv : next_votes_in_round_by_step) {
    next_vote_block_hash = blockWithEnoughVotes_(sv.second);
    if (next_vote_block_hash.second) {
      return next_vote_block_hash;
    }
  }
  return next_vote_block_hash;
}

void PbftManager::placeVote_(taraxa::blk_hash_t const &blockhash,
                             PbftVoteTypes vote_type, uint64_t round,
                             size_t step) {
  if (!shouldSpeak(vote_type, round_, state_)) {
    return;
  }
  auto full_node = node_.lock();
  Vote vote = full_node->generateVote(blockhash, vote_type, round, step,
                                      pbft_chain_last_block_hash_);
  vote_mgr_->addVote(vote);
  LOG(log_deb_) << "vote block hash: " << blockhash
                << " vote type: " << vote_type << " round: " << round
                << " step: " << step << " vote hash " << vote.getHash();
  // pbft vote broadcast
  full_node->broadcastVote(vote);
}

std::pair<blk_hash_t, bool> PbftManager::softVotedBlockForRound_(
    std::vector<taraxa::Vote> &votes, uint64_t round) {
  std::vector<Vote> soft_votes_for_round =
      getVotesOfTypeFromVotesForRoundAndStep_(
          soft_vote_type, votes, round, 2,
          std::make_pair(NULL_BLOCK_HASH, false));

  return blockWithEnoughVotes_(soft_votes_for_round);
}

std::pair<blk_hash_t, bool> PbftManager::proposeOwnBlock_() {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  LOG(log_deb_) << "Into propose PBFT block";
  blk_hash_t last_pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  PbftBlock last_pbft_block;
  std::string last_period_dag_anchor_block_hash;
  if (last_pbft_block_hash) {
    last_pbft_block = pbft_chain_->getPbftBlockInChain(last_pbft_block_hash);
    last_period_dag_anchor_block_hash =
        last_pbft_block.getPivotDagBlockHash().toString();
  } else {
    // First PBFT pivot block
    last_period_dag_anchor_block_hash = dag_genesis_;
  }

  std::vector<std::string> ghost;
  full_node->getGhostPath(last_period_dag_anchor_block_hash, ghost);
  LOG(log_deb_) << "GHOST size " << ghost.size();
  // Looks like ghost never empty, at lease include the last period dag anchor
  // block
  if (ghost.empty()) {
    LOG(log_deb_) << "GHOST is empty. No new DAG blocks generated, PBFT "
                     "propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  blk_hash_t dag_block_hash;
  if (ghost.size() <= DAG_BLOCKS_SIZE) {
    // Move back GHOST_PATH_MOVE_BACK DAG blocks for DAG sycning
    int ghost_index = ghost.size() - 1 - GHOST_PATH_MOVE_BACK;
    if (ghost_index <= 0) {
      ghost_index = 0;
    }
    while (ghost_index < ghost.size() - 1) {
      if (ghost[ghost_index] != last_period_dag_anchor_block_hash) {
        break;
      }
      ghost_index += 1;
    }
    dag_block_hash = blk_hash_t(ghost[ghost_index]);
  } else {
    dag_block_hash = blk_hash_t(ghost[DAG_BLOCKS_SIZE - 1]);
  }
  if (dag_block_hash.toString() == dag_genesis_) {
    LOG(log_deb_) << "No new DAG blocks generated. DAG only has genesis "
                  << dag_block_hash << " PBFT propose NULL_BLOCK_HASH";
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // compare with last dag block hash. If they are same, which means no new
  // dag blocks generated since last round. In that case PBFT proposer should
  // propose NULL BLOCK HASH as their value and not produce a new block. In
  // practice this should never happen
  if (dag_block_hash.toString() == last_period_dag_anchor_block_hash) {
    LOG(log_deb_)
        << "Last period DAG anchor block hash " << dag_block_hash
        << " No new DAG blocks generated, PBFT propose NULL_BLOCK_HASH";
    LOG(log_deb_) << "Ghost: " << ghost;
    return std::make_pair(NULL_BLOCK_HASH, true);
  }
  // get dag blocks hash order
  uint64_t pbft_chain_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(pbft_chain_period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // get dag blocks
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  for (auto const &dag_blk_hash : *dag_blocks_hash_order) {
    auto dag_blk = full_node->getDagBlock(dag_blk_hash);
    assert(dag_blk);
    dag_blocks.emplace_back(dag_blk);
  }

  std::vector<std::vector<std::pair<trx_hash_t, uint>>> dag_blocks_trxs_mode;
  std::unordered_set<trx_hash_t> unique_trxs;
  for (auto const &dag_blk : dag_blocks) {
    // get transactions for each DAG block
    auto trxs_hash = dag_blk->getTrxs();
    std::vector<std::pair<trx_hash_t, uint>> each_dag_blk_trxs_mode;
    for (auto const &t_hash : trxs_hash) {
      if (unique_trxs.find(t_hash) != unique_trxs.end()) {
        // Duplicate transations
        continue;
      }
      unique_trxs.insert(t_hash);
      auto trx = db_->getTransaction(t_hash);
      if (!replay_protection_service_->hasBeenExecuted(*trx)) {
        // TODO: Generate fake transaction schedule, will need pass to VM to
        //  generate the transaction schedule later
        each_dag_blk_trxs_mode.emplace_back(std::make_pair(t_hash, 1));
      }
    }
    dag_blocks_trxs_mode.emplace_back(each_dag_blk_trxs_mode);
  }

  //  TODO: Keep for now to compare, will remove later
  //  // get transactions overlap table
  //  std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
  //      trx_overlap_table =
  //          full_node->computeTransactionOverlapTable(dag_blocks_hash_order);
  //  if (!trx_overlap_table) {
  //    LOG(log_err_) << "Transaction overlap table nullptr, cannot create mock
  //    "
  //                  << "transactions schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  if (trx_overlap_table->empty()) {
  //    LOG(log_deb_) << "Transaction overlap table is empty, no DAG block needs
  //    "
  //                  << " generate mock trx schedule";
  //    return std::make_pair(NULL_BLOCK_HASH, false);
  //  }
  //  // TODO: generate fake transaction schedule for now, will pass
  //  //  trx_overlap_table to VM
  //  std::vector<std::vector<uint>> dag_blocks_trx_modes =
  //      full_node->createMockTrxSchedule(trx_overlap_table);

  TrxSchedule schedule(*dag_blocks_hash_order, dag_blocks_trxs_mode);
  uint64_t propose_pbft_period = pbft_chain_->getPbftChainPeriod() + 1;
  // PBFT chain size as same as last PBFT block height
  uint64_t pbft_block_height = pbft_chain_->getPbftChainSize() + 1;
  addr_t beneficiary = full_node->getAddress();

  // generate generate pbft block
  PbftBlock pbft_block(last_pbft_block_hash, dag_block_hash, schedule,
                       propose_pbft_period, pbft_block_height, beneficiary,
                       full_node->getSecretKey());
  // push pbft block
  pbft_chain_->pushUnverifiedPbftBlock(pbft_block);
  // broadcast pbft block
  std::shared_ptr<Network> network = full_node->getNetwork();
  network->onNewPbftBlock(pbft_block);

  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  LOG(log_deb_) << full_node->getAddress() << " propose PBFT block succussful! "
                << " in round: " << round_ << " in step: " << state_
                << " PBFT block: " << pbft_block;
  return std::make_pair(pbft_block_hash, true);
}

std::pair<blk_hash_t, bool> PbftManager::identifyLeaderBlock_(
    std::vector<Vote> const &votes) {
  LOG(log_deb_) << "Into identify leader block, in round " << round_;
  // each leader candidate with <vote_signature_hash, pbft_block_hash>
  std::vector<std::pair<blk_hash_t, blk_hash_t>> leader_candidates;
  for (auto const &v : votes) {
    if (v.getRound() == round_ && v.getType() == propose_vote_type) {
      // We should not pick aa null block as leader (proposed when
      // no new blocks found, or maliciously) if others have blocks.
      if (round_ == 1 || v.getBlockHash() != NULL_BLOCK_HASH) {
        leader_candidates.emplace_back(
            std::make_pair(dev::sha3(v.getVoteSignature()), v.getBlockHash()));
      }
    }
  }
  if (leader_candidates.empty()) {
    // no eligible leader
    return std::make_pair(NULL_BLOCK_HASH, false);
  }
  std::pair<blk_hash_t, blk_hash_t> leader =
      *std::min_element(leader_candidates.begin(), leader_candidates.end(),
                        [](std::pair<blk_hash_t, blk_hash_t> const &i,
                           std::pair<blk_hash_t, blk_hash_t> const &j) {
                          return i.first < j.first;
                        });

  return std::make_pair(leader.second, true);
}

bool PbftManager::checkPbftBlockValid_(blk_hash_t const &block_hash) const {
  std::pair<PbftBlock, bool> cert_voted_block =
      pbft_chain_->getUnverifiedPbftBlock(block_hash);
  if (!cert_voted_block.second) {
    LOG(log_err_) << "Cannot find the unverified pbft block, block hash "
                  << block_hash;
    return false;
  }
  return pbft_chain_->checkPbftBlockValidation(cert_voted_block.first);
}

bool PbftManager::syncRequestedAlreadyThisStep_() const {
  return round_ == last_sync_round_ && state_ == last_sync_step_;
}

void PbftManager::syncPbftChainFromPeers_() {
  if (!pbft_chain_->pbftSyncedQueueEmpty()) {
    LOG(log_deb_) << "DAG has not synced yet. PBFT chain skips syncing";
    return;
  }

  if (capability_->syncing_ == false) {
    if (syncRequestedAlreadyThisStep_() == false) {
      LOG(log_sil_) << "Restarting pbft sync."
                    << " In round " << round_ << ", in step " << state_
                    << " Send request to ask missing pbft blocks in chain";
      capability_->restartSyncingPbft();
      last_sync_round_ = round_;
      last_sync_step_ = state_;
    }
  }
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    blk_hash_t const &pbft_block_hash) {
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(pbft_block_hash);
  if (!pbft_block.second) {
    LOG(log_deb_) << "Have not got the PBFT block yet. block hash: "
                  << pbft_block_hash;
    return false;
  }

  return comparePbftBlockScheduleWithDAGblocks_(pbft_block.first);
}

bool PbftManager::comparePbftBlockScheduleWithDAGblocks_(
    PbftBlock const &pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable" << std::endl;
    return false;
  }
  uint64_t last_period = pbft_chain_->getPbftChainPeriod();
  // get DAG blocks order
  blk_hash_t dag_block_hash = pbft_block.getPivotDagBlockHash();
  uint64_t pbft_chain_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(pbft_chain_period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // compare DAG blocks hash in PBFT schedule with DAG blocks
  vec_blk_t dag_blocks_hash_in_schedule =
      pbft_block.getSchedule().dag_blks_order;
  if (dag_blocks_hash_in_schedule.size() == dag_blocks_hash_order->size()) {
    for (auto i = 0; i < dag_blocks_hash_in_schedule.size(); i++) {
      if (dag_blocks_hash_in_schedule[i] != (*dag_blocks_hash_order)[i]) {
        LOG(log_inf_) << "DAG blocks have not sync yet. In period: "
                      << last_period << ", DAG block hash "
                      << dag_blocks_hash_in_schedule[i]
                      << " in PBFT schedule is different with DAG block hash "
                      << (*dag_blocks_hash_order)[i];
        return false;
      }
    }
  } else {
    LOG(log_inf_) << "DAG blocks have not sync yet. In period: " << last_period
                  << " PBFT block schedule DAG blocks size: "
                  << dag_blocks_hash_in_schedule.size()
                  << ", DAG blocks size: " << dag_blocks_hash_order->size();
    // For debug
    if (dag_blocks_hash_order->size() > dag_blocks_hash_in_schedule.size()) {
      LOG(log_err_) << "Print DAG blocks order:";
      for (auto const &block : *dag_blocks_hash_order) {
        LOG(log_err_) << "block: " << block;
      }
      LOG(log_err_) << "Print DAG blocks in PBFT schedule order:";
      for (auto const &block : dag_blocks_hash_in_schedule) {
        LOG(log_err_) << "block: " << block;
      }
      std::string filename = "unmatched_pbft_schedule_order_in_period_" +
                             std::to_string(last_period);
      auto addr = full_node->getAddress();
      full_node->drawGraph(addr.toString() + "_" + filename);
      // assert(false);
    }
    return false;
  }

  //  // TODO: may not need to compare transactions, keep it now. If need to
  //  //  compare transactions will need to compare one by one. Would cause
  //  //  performance issue. Below code need to modify.
  //  // compare number of transactions in CS with DAG blocks
  //  // PBFT CS block number of transactions
  //  std::vector<std::vector<uint>> trx_modes =
  //      pbft_block_cs.getScheduleBlock().getSchedule().trxs_mode;
  //  for (int i = 0; i < dag_blocks_hash_order->size(); i++) {
  //    std::shared_ptr<DagBlock> dag_block =
  //        full_node->getDagBlock((*dag_blocks_hash_order)[i]);
  //    // DAG block transations
  //    vec_trx_t dag_block_trxs = dag_block->getTrxs();
  //    if (trx_modes[i].size() != dag_block_trxs.size()) {
  //      LOG(log_err_) << "In DAG block hash: " << (*dag_blocks_hash_order)[i]
  //                    << " has " << dag_block_trxs.size()
  //                    << " transactions. But the DAG block in PBFT CS block "
  //                    << "only has " << trx_modes[i].size() << "
  //                    transactions.";
  //      return false;
  //    }
  //  }

  return true;
}

bool PbftManager::pushCertVotedPbftBlockIntoChain_(
    taraxa::blk_hash_t const &cert_voted_block_hash,
    std::vector<Vote> const &cert_votes_for_round) {
  if (!checkPbftBlockValid_(cert_voted_block_hash)) {
    // Get partition, need send request to get missing pbft blocks from peers
    LOG(log_sil_) << "Cert voted block " << cert_voted_block_hash
                  << " is invalid, we must be out of sync with pbft chain";
    if (capability_->syncing_ == false) {
      syncPbftChainFromPeers_();
    }
    return false;
  }
  std::pair<PbftBlock, bool> pbft_block =
      pbft_chain_->getUnverifiedPbftBlock(cert_voted_block_hash);
  if (!pbft_block.second) {
    LOG(log_err_) << "Can not find the cert vote block hash "
                  << cert_voted_block_hash << " in pbft queue";
    return false;
  }
  if (!comparePbftBlockScheduleWithDAGblocks_(pbft_block.first)) {
    return false;
  }
  if (!pushPbftBlock_(pbft_block.first, cert_votes_for_round)) {
    LOG(log_err_) << "Failed push PBFT block "
                  << pbft_block.first.getBlockHash() << " into chain";
    return false;
  }
  // cleanup PBFT unverified blocks table
  pbft_chain_->cleanupUnverifiedPbftBlocks(pbft_block.first);
  return true;
}

void PbftManager::pushSyncedPbftBlocksIntoChain_() {
  size_t chain_synced_queue_size;
  auto full_node = node_.lock();
  while (!pbft_chain_->pbftSyncedQueueEmpty()) {
    PbftBlockCert pbft_block_and_votes = pbft_chain_->pbftSyncedQueueFront();
    LOG(log_deb_) << "Pick pbft block "
                  << pbft_block_and_votes.pbft_blk.getBlockHash()
                  << " from synced queue in round " << round_;
    if (pbft_chain_->findPbftBlockInChain(
            pbft_block_and_votes.pbft_blk.getBlockHash())) {
      // pushed already from PBFT unverified queue, remove and skip it
      pbft_chain_->pbftSyncedQueuePopFront();

      chain_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (last_chain_synced_queue_size_ != chain_synced_queue_size) {
        LOG(log_deb_) << "PBFT block "
                      << pbft_block_and_votes.pbft_blk.getBlockHash()
                      << " already present in chain.";
        LOG(log_deb_) << "PBFT synced queue still contains "
                      << chain_synced_queue_size
                      << " synced blocks that could not be pushed.";
      }
      last_chain_synced_queue_size_ = chain_synced_queue_size;
      continue;
    }
    // Check cert votes validation
    if (!vote_mgr_->pbftBlockHasEnoughValidCertVotes(
            pbft_block_and_votes, valid_sortition_accounts_size_,
            sortition_threshold_, TWO_T_PLUS_ONE)) {
      // Failed cert votes validation, flush synced PBFT queue and set since
      // next block validation depends on the current one
      LOG(log_err_)
          << "Synced PBFT block "
          << pbft_block_and_votes.pbft_blk.getBlockHash()
          << " doesn't have enough valid cert votes. Clear synced PBFT blocks!";
      pbft_chain_->clearSyncedPbftBlocks();
      break;
    }
    if (!pbft_chain_->checkPbftBlockValidation(pbft_block_and_votes.pbft_blk)) {
      // PBFT chain syncing faster than DAG syncing, wait!
      chain_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
      if (last_chain_synced_queue_size_ != chain_synced_queue_size) {
        LOG(log_deb_) << "PBFT chain unable to push synced block "
                      << pbft_block_and_votes.pbft_blk.getBlockHash();
        LOG(log_deb_) << "PBFT synced queue still contains "
                      << chain_synced_queue_size
                      << " synced blocks that could not be pushed.";
      }
      last_chain_synced_queue_size_ = chain_synced_queue_size;
      break;
    }
    if (!comparePbftBlockScheduleWithDAGblocks_(
            pbft_block_and_votes.pbft_blk)) {
      break;
    }
    if (!pushPbftBlock_(pbft_block_and_votes.pbft_blk,
                        pbft_block_and_votes.cert_votes)) {
      LOG(log_err_) << "Failed push PBFT block "
                    << pbft_block_and_votes.pbft_blk.getBlockHash()
                    << " into chain";
      break;
    }
    // Remove from PBFT synced queue
    pbft_chain_->pbftSyncedQueuePopFront();
    if (have_chained_block_) {
      last_chain_period_ = pbft_chain_->getPbftChainPeriod();
      // Update sortition accounts table
      updateSortitionAccountsTable_();
      // update sortition_threshold and TWO_T_PLUS_ONE
      updateTwoTPlusOneAndThreshold_();
      have_chained_block_ = false;
    }
    chain_synced_queue_size = pbft_chain_->pbftSyncedQueueSize();
    if (last_chain_synced_queue_size_ != chain_synced_queue_size) {
      LOG(log_deb_) << "PBFT synced queue still contains "
                    << chain_synced_queue_size
                    << " synced blocks that could not be pushed.";
    }
    last_chain_synced_queue_size_ = chain_synced_queue_size;

    // Since we pushed via syncing we should reset this...
    next_voted_block_from_previous_round_ =
        std::make_pair(NULL_BLOCK_HASH, false);
  }
}

bool PbftManager::pushPbftBlock_(PbftBlock const &pbft_block,
                                 std::vector<Vote> const &cert_votes) {
  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  if (db_->pbftBlockInDb(pbft_block_hash)) {
    LOG(log_err_) << "PBFT block: " << pbft_block_hash << " in DB already";
    return false;
  }
  auto batch = db_->createWriteBatch();
  // execute PBFT schedule
  auto full_node = node_.lock();
  if (!full_node->executePeriod(batch, pbft_block,
                                sortition_account_balance_table_tmp)) {
    LOG(log_err_) << "Failed to execute PBFT schedule. PBFT Block: "
                  << pbft_block;
    for (auto const &v : cert_votes) {
      LOG(log_err_) << "Cert vote: " << v;
    }
    return false;
  }
  // Update number of executed DAG blocks/transactions in DB
  auto executor = full_node->getExecutor();
  auto num_executed_blk = executor->getNumExecutedBlk();
  auto num_executed_trx = executor->getNumExecutedTrx();
  if (num_executed_blk > 0 && num_executed_trx > 0) {
    db_->addStatusFieldToBatch(StatusDbField::ExecutedBlkCount,
                               num_executed_blk, batch);
    db_->addStatusFieldToBatch(StatusDbField::ExecutedTrxCount,
                               num_executed_trx, batch);
  }
  // Add dag_block_period in DB
  uint64_t pbft_period = pbft_block.getPeriod();
  for (auto const blk_hash : pbft_block.getSchedule().dag_blks_order) {
    db_->addDagBlockPeriodToBatch(blk_hash, pbft_period, batch);
  }
  // Get dag blocks order
  blk_hash_t dag_block_hash = pbft_block.getPivotDagBlockHash();
  uint64_t current_period;
  std::shared_ptr<vec_blk_t> dag_blocks_hash_order;
  std::tie(current_period, dag_blocks_hash_order) =
      full_node->getDagBlockOrder(dag_block_hash);
  // Add DAG blocks order and DAG blocks height in DB
  uint64_t max_dag_blocks_height = pbft_chain_->getDagBlockMaxHeight();
  for (auto const &dag_blk_hash : *dag_blocks_hash_order) {
    auto dag_block_height_ptr = db_->getDagBlockHeight(dag_blk_hash);
    if (dag_block_height_ptr) {
      LOG(log_err_) << "Duplicate DAG block " << dag_blk_hash
                    << " in DAG blocks height DB already";
      continue;
    }
    max_dag_blocks_height++;
    db_->addDagBlockOrderAndHeightToBatch(dag_blk_hash, max_dag_blocks_height,
                                          batch);
    LOG(log_deb_) << "Add dag block " << dag_blk_hash << " with height "
                  << max_dag_blocks_height << " in DB write batch";
  }
  // Add cert votes in DB
  db_->addPbftCertVotesToBatch(pbft_block_hash, cert_votes, batch);
  LOG(log_deb_) << "Storing cert votes of pbft blk " << pbft_block_hash << "\n"
                << cert_votes;
  // Add PBFT block in DB
  db_->addPbftBlockToBatch(pbft_block, batch);
  // Add PBFT block index in DB
  auto pbft_block_index = pbft_block.getHeight();
  db_->addPbftBlockIndexToBatch(pbft_block_index, pbft_block_hash, batch);
  // Update period_schedule_block in DB
  db_->addPbftBlockPeriodToBatch(pbft_period, pbft_block_hash, batch);
  // Update sortition account balance table DB
  updateSortitionAccountsDB_(batch);
  // TODO: Should remove PBFT chain head from DB
  // Update pbft chain
  // TODO: after remove PBFT chain head from DB, update pbft chain should after
  //  DB commit
  pbft_chain_->updatePbftChain(pbft_block_hash);
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain_->getGenesisHash();
  std::string pbft_chain_head_str = pbft_chain_->getJsonStr();
  db_->addPbftChainHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str,
                               batch);
  // Commit DB
  db_->commitWriteBatch(batch);
  LOG(log_deb_) << "DB write batch committed already";

  // Set DAG blocks period
  full_node->setDagBlockOrder(dag_block_hash, pbft_period);

  // Set max DAG block height
  pbft_chain_->setDagBlockMaxHeight(max_dag_blocks_height);

  // Reset proposed PBFT block hash to False for next pbft block proposal
  proposed_block_hash_ = std::make_pair(NULL_BLOCK_HASH, false);
  have_chained_block_ = true;

  // Update web server
  full_node->updateWsScheduleBlockExecuted(pbft_block);

  // Update Eth service head
  full_node->getEthService()->update_head();

  LOG(log_inf_) << full_node->getAddress() << " successful push pbft block "
                << pbft_block_hash << " in period " << pbft_period
                << " into chain! In round " << round_;
  return true;
}

void PbftManager::updateTwoTPlusOneAndThreshold_() {
  uint64_t last_pbft_period = pbft_chain_->getPbftChainPeriod();
  int64_t since_period;
  if (last_pbft_period < SKIP_PERIODS) {
    since_period = 0;
  } else {
    since_period = last_pbft_period - SKIP_PERIODS;
  }
  size_t active_players = 0;
  while (active_players == 0 && since_period >= 0) {
    active_players += std::count_if(
        sortition_account_balance_table.begin(),
        sortition_account_balance_table.end(),
        [since_period](std::pair<const taraxa::addr_t,
                                 taraxa::PbftSortitionAccount> const &account) {
          return (account.second.last_period_seen >= since_period);
        });
    if (active_players == 0) {
      LOG(log_war_) << "Active players was found to be 0 since period "
                    << since_period
                    << ". Will go back one period continue to check. ";
      since_period--;
    }
  }
  auto full_node = node_.lock();
  addr_t account_address = full_node->getAddress();
  is_active_player_ =
      sortition_account_balance_table[account_address].last_period_seen >=
      since_period;
  if (active_players == 0) {
    // IF active_players count 0 then all players should be treated as active
    LOG(log_war_) << "Active players was found to be 0! This should only "
                     "happen at initialization, when master boot node "
                     "distribute all of coins out to players. Will set active "
                     "player count to be sortition table size of "
                  << valid_sortition_accounts_size_ << ". Last period is "
                  << last_pbft_period;
    active_players = valid_sortition_accounts_size_;
    is_active_player_ = true;
  }

  // Update 2t+1 and threshold
  if (COMMITTEE_SIZE <= active_players) {
    TWO_T_PLUS_ONE = COMMITTEE_SIZE * 2 / 3 + 1;
    // round up
    sortition_threshold_ =
        (valid_sortition_accounts_size_ * COMMITTEE_SIZE - 1) / active_players +
        1;
  } else {
    TWO_T_PLUS_ONE = active_players * 2 / 3 + 1;
    sortition_threshold_ = valid_sortition_accounts_size_;
  }
  LOG(log_inf_) << "Update 2t+1 " << TWO_T_PLUS_ONE << ", Threshold "
                << sortition_threshold_ << ", valid voting players "
                << valid_sortition_accounts_size_ << ", active players "
                << active_players << " since period " << since_period;
}

void PbftManager::updateSortitionAccountsTable_() {
  sortition_account_balance_table.clear();
  for (auto &account : sortition_account_balance_table_tmp) {
    sortition_account_balance_table[account.first] = account.second;
  }
  // update sortition accounts size
  valid_sortition_accounts_size_ = sortition_account_balance_table.size();
}

void PbftManager::updateSortitionAccountsDB_(DbStorage::BatchPtr const &batch) {
  for (auto &account : sortition_account_balance_table_tmp) {
    if (account.second.status == new_change) {
      db_->addSortitionAccountToBatch(account.first, account.second, batch);
      account.second.status = updated;
    } else if (account.second.status == remove) {
      // Erase both from DB and cache(table)
      db_->removeSortitionAccount(account.first);
      sortition_account_balance_table_tmp.erase(account.first);
    }
  }
  auto account_size =
      std::to_string(sortition_account_balance_table_tmp.size());
  db_->addSortitionAccountToBatch(std::string("sortition_accounts_size"),
                                  account_size, batch);
}

void PbftManager::countVotes_() {
  while (!stop_monitor_) {
    std::vector<Vote> votes = vote_mgr_->getAllVotes();

    size_t num_votes = 0;
    size_t num_last_votes = 0;
    for (auto const &v : votes) {
      if (v.getRound() == round_ && v.getStep() == step_) {
        num_votes++;
      } else if (v.getRound() == last_round_ && v.getStep() == last_step_) {
        num_last_votes++;
      }
    }

    LOG(log_inf_test_) << "Round " << round_ << " step " << last_state_
                       << " time " << getLastStateElapsedTimeMs() << "(ms) has "
                       << num_last_votes << " votes";
    LOG(log_inf_test_) << "Round " << round_ << " step " << state_ << " time "
                       << getStateElapsedTimeMs() << "(ms) has " << num_votes
                       << " votes";
    thisThreadSleepForMilliSeconds(100);
  }
}

uint64_t PbftManager::getNumStepsOverMax() const {
  return (step_ <= MAX_STEPS) ? 0 : (MAX_STEPS - step_);
}

u_long PbftManager::getRoundElapsedTimeMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() - round_start_clock_time_)
      .count();
}

u_long PbftManager::getStateElapsedTimeMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() - state_start_clock_time_)
      .count();
}

u_long PbftManager::getLastStateElapsedTimeMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() - last_state_start_clock_time_)
      .count();
}

u_long PbftManager::getNextStateCheckTimeMs() const {
  if (state_ == chain_block_state) {
    return 0;
  }
  u_long elapsed_time = getStateElapsedTimeMs();
  if (elapsed_time >= state_max_time_ms_) {
    return 0;
  }
  u_long sleep_time_ms = state_max_time_ms_ - elapsed_time;
  if (!isPollingState(state_)) {
    return sleep_time_ms;
  }
  // Polling is only done for the last 2 lambda ms
  ulong polling_period_ms = 2 * state_lambda_ms_;
  if (sleep_time_ms > polling_period_ms) {
    return state_max_time_ms_ - polling_period_ms;
  }
  if (sleep_time_ms > POLLING_INTERVAL_ms) {
    return POLLING_INTERVAL_ms;
  }
  return sleep_time_ms;
}

bool PbftManager::hasNextVotedBlockFromLastRound() const {
  return !(chained_blocks_for_round_.count(round_ - 1) ||
           (next_voted_block_from_previous_round_.second &&
            next_voted_block_from_previous_round_.first == NULL_BLOCK_HASH));
}

bool PbftManager::hasChainedBlockFromRound() const {
  return chained_blocks_for_round_.count(round_);
}

void PbftManager::resetRound() {
  setRound(1);
  last_round_ = 0;

  // Initialize TWO_T_PLUS_ONE and sortition_threshold
  updateTwoTPlusOneAndThreshold_();

  // Reset last sync request point...
  last_sync_round_ = 0;
  last_sync_step_ = 0;
}

void PbftManager::setRound(uint64_t round) {
  last_round_ = round_;
  round_ = round;
  resetState();

  last_round_start_clock_time_ = round_start_clock_time_;
  round_start_clock_time_ = std::chrono::system_clock::now();

  own_starting_value_for_round_ = NULL_BLOCK_HASH;

  have_next_voted_soft_value_ = false;
  have_next_voted_null_block_hash_ = false;
  have_cert_voted_this_round_ = false;

  // Key thing is to set .second to false to mark that we have not
  // identified a soft voted block in the new upcoming round...
  soft_voted_block_for_this_round_ = std::make_pair(NULL_BLOCK_HASH, false);

  // Identify what block was next voted if any in this last round...
  next_voted_block_from_previous_round_ =
      nextVotedBlockForRoundAndStep_(round_votes_, last_round_);

  if (have_chained_block_) {
    last_chain_period_ = pbft_chain_->getPbftChainPeriod();
    // Update sortition accounts table
    updateSortitionAccountsTable_();
    // reset sortition_threshold and TWO_T_PLUS_ONE
    updateTwoTPlusOneAndThreshold_();
    have_chained_block_ = false;
  }

  // Update pbft chain last block hash at start of new round...
  pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();
  // END ROUND START STATE CHANGE UPDATES

  // p2p connection syncing should cover this situation, sync here for safe
  if (round_ > last_round_ + 1 && capability_->syncing_ == false) {
    LOG(log_sil_) << "Quorum determined round " << round_
                  << " > 1 + current round " << last_round_
                  << " local round, need to broadcast request for missing "
                     "certified blocks";

    // NOTE: Advance round and step before calling sync to make sure
    //       sync call won't be supressed for being too
    //       recent (ie. same round and step)
    syncPbftChainFromPeers_();

    next_voted_block_from_previous_round_ =
        std::make_pair(NULL_BLOCK_HASH, false);
  }
  round_votes_ = vote_mgr_->getVotes(round_, valid_sortition_accounts_size_,
                                     sync_peers_pbft_chain_);
  LOG(log_tra_) << "There are " << round_votes_.size()
                << " total votes in round " << round_;
}

void PbftManager::resetState() {
  step_ = 0;
  setState(reset_state);
}

void PbftManager::setState(int state) {
  if ((state != reset_state) && (state == state_)) {
    return;
  }
  last_step_ = step_;
  step_ = (state == reset_state) ? 0 : step_ + 1;
  last_state_ = (state == reset_state) ? reset_state : state_;
  state_ = state;
  last_state_start_clock_time_ = state_start_clock_time_;
  state_start_clock_time_ = std::chrono::system_clock::now();

  state_lambda_ms_ = LAMBDA_ms_MIN;
  state_max_time_ms_ = 2 * state_lambda_ms_;
  if (state_ >= soft_vote_block_from_leader) {
    state_max_time_ms_ += 4 * state_lambda_ms_;
  }
  if (state_ >= cert_vote_block_polling_state) {
    state_max_time_ms_ += 2 * state_lambda_ms_;
  }
  if (state_ >= chain_block_state) {
    auto num_steps_over_max = getNumStepsOverMax();
    if (!num_steps_over_max) {
      state_max_time_ms_ += (step_ - 3) * state_lambda_ms_;
    } else {
      state_max_time_ms_ += (MAX_STEPS - 3) * state_lambda_ms_;
      state_lambda_ms_ = 100 * LAMBDA_ms_MIN;
      LOG(log_inf_) << "Surpassed max steps, relaxing lambda to "
                    << state_lambda_ms_ << " ms in round " << round_
                    << ", step " << state_;
      state_max_time_ms_ += num_steps_over_max * state_lambda_ms_;
    }
  }
}

void PbftManager::syncChain() {
  // NOTE: PUSHING OF SYNCED BLOCKS CAN TAKE A LONG TIME
  //       SHOULD DO BEFORE WE SET THE ELAPSED TIME IN ROUND
  // push synced pbft blocks into chain
  pushSyncedPbftBlocksIntoChain_();
  // update pbft chain last block hash
  pbft_chain_last_block_hash_ = pbft_chain_->getLastPbftBlockHash();

  // Get votes
  sync_peers_pbft_chain_ = false;
  round_votes_ = vote_mgr_->getVotes(round_, valid_sortition_accounts_size_,
                                     sync_peers_pbft_chain_);
  LOG(log_tra_) << "There are " << round_votes_.size()
                << " total votes in round " << round_;

  // Concern can malicious node trigger excessive syncing?
  if (sync_peers_pbft_chain_ && pbft_chain_->pbftSyncedQueueEmpty() &&
      capability_->syncing_ == false &&
      syncRequestedAlreadyThisStep_() == false) {
    LOG(log_sil_) << "Vote validation triggered pbft chain sync";
    syncPbftChainFromPeers_();
  }

  // Check if we are synced to the right step _
  uint64_t consensus_pbft_round = roundDeterminedFromVotes_(round_votes_);

  // This should be always true...
  assert(consensus_pbft_round >= round_);

  if (consensus_pbft_round > round_) {
    LOG(log_inf_) << "From votes determined round " << consensus_pbft_round;
    setRound(consensus_pbft_round);
    LOG(log_deb_) << "Advancing clock to pbft round " << round_
                  << ", step 1, and resetting clock.";
  }
}

void PbftManager::executeState() {
  LOG(log_tra_) << "PBFT current round is " << round_;
  LOG(log_tra_) << "PBFT current step is " << state_;

  switch (state_) {
    case reset_state:
      break;
    case roll_call_state: {
      LOG(log_sil_) << "Proposing value of NULL_BLOCK_HASH " << NULL_BLOCK_HASH
                    << " for round 1 by protocol";
      placeVote_(own_starting_value_for_round_, propose_vote_type, round_,
                 step_);
    } break;
    case propose_block_from_self_state: {
      // PBFT block must only be proposed once in one period
      if (!proposed_block_hash_.second ||
          proposed_block_hash_.first == NULL_BLOCK_HASH) {
        proposed_block_hash_ = proposeOwnBlock_();
      }
      if (proposed_block_hash_.second) {
        own_starting_value_for_round_ = proposed_block_hash_.first;
        LOG(log_sil_) << "Proposing own starting value "
                      << own_starting_value_for_round_ << " for round "
                      << round_;
        placeVote_(proposed_block_hash_.first, propose_vote_type, round_,
                   step_);
      }

    } break;
    case propose_block_from_prev_round_state: {
      if (next_voted_block_from_previous_round_.second &&
          next_voted_block_from_previous_round_.first != NULL_BLOCK_HASH) {
        own_starting_value_for_round_ =
            next_voted_block_from_previous_round_.first;
        LOG(log_sil_) << "Proposing next voted block "
                      << next_voted_block_from_previous_round_.first
                      << " from previous round, for round " << round_;
        placeVote_(next_voted_block_from_previous_round_.first,
                   propose_vote_type, round_, step_);
      }
    } break;
    case soft_vote_block_from_leader: {
      // Identity leader
      {
        std::pair<blk_hash_t, bool> leader_block =
            identifyLeaderBlock_(round_votes_);
        if (leader_block.second) {
          own_starting_value_for_round_ = leader_block.first;
          LOG(log_deb_) << "Identify leader block " << leader_block.first
                        << " at round " << round_;
          if (shouldSpeak(soft_vote_type, round_, step_)) {
            LOG(log_deb_) << "Soft voting block " << leader_block.first
                          << " at round " << round_;
            placeVote_(leader_block.first, soft_vote_type, round_, step_);
          }
        }
      }
    } break;
    case soft_vote_block_from_prev_round_state: {
      LOG(log_deb_) << "Soft voting "
                    << next_voted_block_from_previous_round_.first
                    << " from previous round";
      placeVote_(next_voted_block_from_previous_round_.first, soft_vote_type,
                 round_, step_);
    } break;
    case cert_vote_block_polling_state: {
      // The Certifying Step
      if (getRoundElapsedTimeMs() < (2 * LAMBDA_ms_MIN)) {
        // Should not happen, add log here for safety checking
        LOG(log_err_) << "PBFT Reached step 3 too quickly after only "
                      << getRoundElapsedTimeMs() << " (ms) in round "
                      << round_;
      }

      if (!have_cert_voted_this_round_) {
        LOG(log_tra_) << "In step 3";
        if (!soft_voted_block_for_this_round_.second) {
          soft_voted_block_for_this_round_ =
              softVotedBlockForRound_(round_votes_, round_);
        }

        if (soft_voted_block_for_this_round_.second &&
            soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH &&
            comparePbftBlockScheduleWithDAGblocks_(
                soft_voted_block_for_this_round_.first)) {
          LOG(log_tra_) << "Finished comparePbftBlockScheduleWithDAGblocks_";

          // NOTE: If we have already executed this round
          //       then block won't be found in unverified queue...
          bool executed_soft_voted_block_for_this_round = false;
          if (hasChainedBlockFromRound()) {
            LOG(log_tra_) << "Have already executed before certifying in "
                             "step 3 in round "
                          << round_;
            if (pbft_chain_->getLastPbftBlockHash() ==
                soft_voted_block_for_this_round_.first) {
              LOG(log_tra_) << "Having executed, last block in chain is the "
                               "soft voted block in round "
                            << round_;
              executed_soft_voted_block_for_this_round = true;
            }
          }

          bool unverified_soft_vote_block_for_this_round_is_valid = false;
          if (!executed_soft_voted_block_for_this_round) {
            if (checkPbftBlockValid_(soft_voted_block_for_this_round_.first)) {
              LOG(log_tra_) << "checkPbftBlockValid_ returned true";
              unverified_soft_vote_block_for_this_round_is_valid = true;
            } else {
              // Get partition, need send request to get missing pbft
              // blocks from peers
              LOG(log_sil_) << "Soft voted block for this round appears "
                               "to be invalid, "
                               "we must be out of sync with pbft chain";
              if (!capability_->syncing_) {
                syncPbftChainFromPeers_();
              }
            }
          }

          if (executed_soft_voted_block_for_this_round ||
              unverified_soft_vote_block_for_this_round_is_valid) {
            cert_voted_values_for_round_[round_] =
                soft_voted_block_for_this_round_.first;

            // NEED TO KEEP POLLING TO SEE IF WE HAVE 2t+1 cert votes...
            // Here we would cert vote if we can speak....
            have_cert_voted_this_round_ = true;
            LOG(log_deb_) << "Cert voting "
                          << soft_voted_block_for_this_round_.first
                          << " for round " << round_;
            // generate cert vote
            placeVote_(soft_voted_block_for_this_round_.first, cert_vote_type,
                       round_, step_);
          }
        }
      }
    } break;
    case chain_block_state: {
      // CHECK IF WE HAVE RECEIVED 2t+1 CERT VOTES FOR A BLOCK IN OUR
      // CURRENT ROUND.  IF WE HAVE THEN WE PUSH THE BLOCK ONTO THE CHAIN
      // ONLY CHECK IF HAVE *NOT* YET CHAINED A BLOCK THIS ROUND...
      std::vector<Vote> cert_votes_for_round =
          getVotesOfTypeFromVotesForRoundAndStep_(
              cert_vote_type, round_votes_, round_, 3,
              std::make_pair(NULL_BLOCK_HASH, false));
      // TODO: debug remove later
      LOG(log_tra_) << "Get cert votes for round " << round_ << " step "
                    << state_;
      std::pair<blk_hash_t, bool> cert_voted_block_hash =
          blockWithEnoughVotes_(cert_votes_for_round);
      if (cert_voted_block_hash.second) {
        LOG(log_deb_) << "PBFT block " << cert_voted_block_hash.first
                      << " has enough certed votes";
        // put pbft block into chain
        if (pushCertVotedPbftBlockIntoChain_(cert_voted_block_hash.first,
                                             cert_votes_for_round)) {
          chained_blocks_for_round_[round_] = cert_voted_block_hash.first;
          LOG(log_sil_) << "Write " << cert_votes_for_round.size()
                        << " votes ... in round " << round_;
          // TODO: debug remove later
          LOG(log_deb_) << "The cert voted pbft block is "
                        << cert_voted_block_hash.first;
          LOG(log_deb_) << "Pushing PBFT block and Execution spent "
                        << getStateElapsedTimeMs() << " ms. in round "
                        << round_;
        }
      }
    } break;
    case vote_next_block_state: {
      if (cert_voted_values_for_round_.find(round_) !=
          cert_voted_values_for_round_.end()) {
        LOG(log_deb_) << "Next voting cert voted value "
                      << cert_voted_values_for_round_[round_] << " for round "
                      << round_;
        placeVote_(cert_voted_values_for_round_[round_], next_vote_type, round_,
                   step_);
      } else if (round_ >= 2 && next_voted_block_from_previous_round_.second &&
                 next_voted_block_from_previous_round_.first ==
                     NULL_BLOCK_HASH) {
        LOG(log_deb_) << "Next voting NULL BLOCK for round " << round_;
        placeVote_(NULL_BLOCK_HASH, next_vote_type, round_, step_);
      } else {
        LOG(log_deb_) << "Next voting nodes own starting value "
                      << own_starting_value_for_round_ << " for round "
                      << round_;
        placeVote_(own_starting_value_for_round_, next_vote_type, round_,
                   step_);
      }
      have_next_voted_soft_value_ = false;
      have_next_voted_null_block_hash_ = false;
    } break;
    case vote_next_block_polling_state: {
      if (!soft_voted_block_for_this_round_.second) {
        soft_voted_block_for_this_round_ =
            softVotedBlockForRound_(round_votes_, round_);
      }
      if (!have_next_voted_soft_value_ && soft_voted_block_for_this_round_.second &&
          soft_voted_block_for_this_round_.first != NULL_BLOCK_HASH) {
        LOG(log_deb_) << "Next voting "
                      << soft_voted_block_for_this_round_.first << " for round "
                      << round_;
        placeVote_(soft_voted_block_for_this_round_.first, next_vote_type,
                   round_, step_);
        have_next_voted_soft_value_ = true;
      }
      if (!have_next_voted_null_block_hash_ && round_ >= 2 &&
          next_voted_block_from_previous_round_.second &&
          // next_voted_block_from_previous_round_.first ==
          // NULL_BLOCK_HASH &&
          (cert_voted_values_for_round_.find(round_) ==
           cert_voted_values_for_round_.end())) {
        LOG(log_deb_) << "Next voting NULL BLOCK for round " << round_
                      << " step " << step_;
        placeVote_(NULL_BLOCK_HASH, next_vote_type, round_, step_);
        have_next_voted_null_block_hash_ = true;
      }
    } break;
    default: {
      LOG(log_err_) << "Invalid PBFT step: " << state_;
      syncPbftChainFromPeers_();
    } break;
  }

  if (getNumStepsOverMax() && !capability_->syncing_ &&
      !syncRequestedAlreadyThisStep_()) {
    LOG(log_war_) << "Suspect pbft chain behind, inaccurate 2t+1, need "
                     "to broadcast request for missing blocks";
    syncPbftChainFromPeers_();
  }
}

bool PbftManager::isPollingState(int state) {
  return (state == cert_vote_block_polling_state) ||
         (state == vote_next_block_polling_state);
}

}  // namespace taraxa
