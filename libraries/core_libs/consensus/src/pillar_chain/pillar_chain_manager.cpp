#include "pillar_chain/pillar_chain_manager.hpp"

#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "vote_manager/vote_manager.hpp"

// TODO: should ne #include <libBLS/tools/utils.h>
#include <tools/utils.h>

namespace taraxa {

PillarChainManager::PillarChainManager(std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<KeyManager> key_manager,
                                       const libff::alt_bn128_Fr& bls_secret_key, addr_t node_addr)
    : db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      vote_mgr_(std::move(vote_mgr)),
      key_manager_(std::move(key_manager)),
      node_addr_(node_addr),
      kBlsSecretKey(bls_secret_key),
      last_pillar_block_{nullptr},
      last_pillar_block_signatures_{},
      mutex_{} {
  libBLS::ThresholdUtils::initCurve();

  // TODO: load last_pillar_block_ from db

  LOG_OBJECTS_CREATE("PILLAR_CHAIN");
}

void PillarChainManager::newFinalBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data) {
  const auto block_num = block_data->final_chain_blk->number;
  // There should always be last_pillar_block_, except for the very first pillar block
  assert(block_num <= kEpochBlocksNum || last_pillar_block_);

  // TODO: add each final block to final blocks merkle tree
  const auto epoch_blocks_merkle_root = block_data->final_chain_blk->hash;

  // Create pillar block and broadcast BLS signature
  if (block_num % kEpochBlocksNum == 0) {
    // TODO: this will not work for light node
    // Get validators stakes changes between the current and previous pillar block
    auto stakes_changes = getOrderedValidatorsStakesChanges(block_num);

    const auto pillar_block = std::make_shared<PillarBlock>(block_num, epoch_blocks_merkle_root,
                                                            std::move(stakes_changes), last_pillar_block_->getHash());

    // Get 2t+1 threshold
    // Note: do not use signature->getPeriod() - 1 as in votes processing - signature are made after the block is
    // finalized, not before as votes
    const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(pillar_block->getPeriod(), PbftVoteTypes::cert_vote);
    // getPbftTwoTPlusOne returns empty optional only when requested period is too far ahead and that should never
    // happen as newFinalBlock is triggered only after the block is finalized
    assert(two_t_plus_one.has_value());

    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_pillar_block_ = pillar_block;
    last_pillar_block_signatures_.clear();
    last_pillar_block_signatures_weight_ = 0;
    if (two_t_plus_one.has_value()) [[likely]] {
      last_pillar_block_two_t_plus_one_ = *two_t_plus_one;
    }

    return;
  }

  // TODO: do not create & gossip own sig or request other signatures if the node is in syncing state
  // Create and broadcast own bls signature
  if (block_num % (kEpochBlocksNum + kBlsSigBroadcastDelayBlocks) == 0) {
    // Check if node is eligible validator
    try {
      if (!final_chain_->dpos_is_eligible(block_num, node_addr_)) {
        return;
      }
    } catch (state_api::ErrFutureBlock& e) {
      assert(false);  // This should never happen as newFinalBlock is triggered after the new block is finalized
      return;
    }

    if (!key_manager_->getBlsKey(block_num, node_addr_)) {
      LOG(log_er_) << "No bls public key registered in dpos contract !";
      return;
    }

    // Create and broadcast bls signature with kBlsSigBroadcastDelayBlocks delay to make sure other up-to-date nodes
    // already processed the latest pillar block
    // TODO: maybe dont use the delay and accept signatures with right period ???
    std::shared_ptr<BlsSignature> signature;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      signature = std::make_shared<BlsSignature>(last_pillar_block_->getHash(), block_num, node_addr_, kBlsSecretKey);
    }

    addVerifiedBlsSig(signature);

    if (auto net = network_.lock()) {
      net->gossipBlsSignature(signature);
    }

    return;
  }

  // Check (& request) 2t+1 signatures
  // TODO: fix bls sigs requesting - do it every N blocks after pillar block was created
  if (block_num % kCheckLatestBlockBlsSigs == 0) {
    PillarBlock::Hash last_pillar_block_hash;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      last_pillar_block_hash = last_pillar_block_->getHash();
    }

    // TODO: Request signature only if node does not have 2t+1 bls signatures
    if (auto net = network_.lock()) {
      net->requestBlsSigBundle(last_pillar_block_hash);
    }
  }
}

bool PillarChainManager::isRelevantBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  if (last_pillar_block_->getHash() != signature->getPillarBlockHash()) {
    return false;
  }

  if (last_pillar_block_signatures_.contains(signature->getHash())) {
    return false;
  }

  return true;
}

bool PillarChainManager::validateBlsSignature(const std::shared_ptr<BlsSignature> signature) const {
  const auto sig_period = signature->getPeriod();
  const auto sig_author = signature->getSignerAddr();

  // Check if signer registered his bls key
  const auto bls_pub_key = key_manager_->getBlsKey(sig_period, sig_author);
  if (!bls_pub_key) {
    LOG(log_er_) << "No bls public key mapped for node " << sig_author << ". Signature " << signature->getHash();
    return false;
  }

  // Check if signer is eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(sig_period, sig_author)) {
      LOG(log_er_) << "Signer is not eligible validator. Signature " << signature->getHash();
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_wr_) << "Period " << sig_period << " is too far ahead of DPOS. Signature " << signature->getHash()
                 << ". Err: " << e.what();
    return false;
  }

  if (!signature->isValid(bls_pub_key)) {
    LOG(log_er_) << "Invalid signature " << signature->getHash();
    return false;
  }

  return true;
}

bool PillarChainManager::addVerifiedBlsSig(const std::shared_ptr<BlsSignature>& signature) {
  uint64_t signer_vote_count = 0;
  try {
    signer_vote_count = final_chain_->dpos_eligible_vote_count(signature->getPeriod(), signature->getSignerAddr());
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Signature " << signature->getHash() << " period " << signature->getPeriod()
                 << " is too far ahead of DPOS. " << e.what();
    return false;
  }

  if (!signer_vote_count) {
    // Signer is not a valid validator
    LOG(log_er_) << "Signature " << signature->getHash() << " author's " << signature->getSignerAddr()
                 << " stake is zero";
    return false;
  }

  std::vector<libff::alt_bn128_G1> bls_signatures;
  dev::RLPStream signers_addresses_rlp;
  PillarBlock::Hash pillar_block_hash;

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    if (!last_pillar_block_signatures_.emplace(std::make_pair(signature->getHash(), signature)).second) {
      // Signature is already saved
      return true;
    }

    if ((last_pillar_block_signatures_weight_ += signer_vote_count) < last_pillar_block_two_t_plus_one_) {
      // Signatures weight < 2t+1
      return true;
    }

    pillar_block_hash = last_pillar_block_signatures_.begin()->second->getPillarBlockHash();

    signers_addresses_rlp.appendList(last_pillar_block_signatures_.size());
    for (const auto& bls_sig : last_pillar_block_signatures_) {
      signers_addresses_rlp << bls_sig.second->getSignerAddr();
      bls_signatures.push_back(bls_sig.second->getSignature());
    }
  }

  // Create aggregated bls signature and save it to db
  libff::alt_bn128_G1 aggregated_signature = libBLS::Bls::Aggregate(bls_signatures);

  std::stringstream aggregated_signature_ss;
  aggregated_signature_ss << aggregated_signature;

  dev::RLPStream bls_aggregated_sig_rlp(3);
  bls_aggregated_sig_rlp << pillar_block_hash;
  bls_aggregated_sig_rlp << aggregated_signature_ss.str();
  bls_aggregated_sig_rlp.appendRaw(signers_addresses_rlp.invalidate());

  // TODO: save bls_aggregated_sig_rlp to db

  return true;
}

std::vector<std::shared_ptr<BlsSignature>> PillarChainManager::getVerifiedBlsSignatures(
    const PillarBlock::Hash pillar_block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (pillar_block_hash != last_pillar_block_->getHash()) {
    return {};
  }

  std::vector<std::shared_ptr<BlsSignature>> signatures;
  signatures.reserve(last_pillar_block_signatures_.size());
  for (const auto& sig : last_pillar_block_signatures_) {
    signatures.push_back(sig.second);
  }

  return signatures;
}

std::vector<PillarBlock::ValidatorStakeChange> PillarChainManager::getOrderedValidatorsStakesChanges(
    EthBlockNumber block) const {
  assert(block % kEpochBlocksNum == 0);

  auto current_stakes = final_chain_->dpos_validators_total_stakes(block);

  // First pillar block - return all current stakes
  if (!last_pillar_block_) {
    std::vector<PillarBlock::ValidatorStakeChange> changes;
    changes.reserve(current_stakes.size());
    std::transform(current_stakes.begin(), current_stakes.end(), std::back_inserter(changes), [](auto& stake) {
      return PillarBlock::ValidatorStakeChange{stake.addr, stake.stake};
    });

    return changes;
  }

  auto transformToMap = [](const std::vector<state_api::ValidatorStake>& changes) {
    std::map<addr_t, PillarBlock::ValidatorStakeChange> changes_map;
    for (const auto& change : changes) {
      // Convert ValidatorStake.stake uint256 to ValidatorStakeChange.stake int256
      changes_map[change.addr] = PillarBlock::ValidatorStakeChange{change.addr, change.stake};
    }

    return changes_map;
  };

  auto previous_stakes = final_chain_->dpos_validators_total_stakes(last_pillar_block_->getPeriod());

  auto current_stakes_map = transformToMap(current_stakes);
  auto previous_stakes_map = transformToMap(previous_stakes);

  // First create ordered map so the changes are ordered by validator addresses
  std::map<addr_t, PillarBlock::ValidatorStakeChange> changes_map;
  for (auto& current_stake : current_stakes_map) {
    auto previous_stake = previous_stakes_map.find(current_stake.first);

    // Previous stakes does not contain validator address from current stakes -> new stake(delegator)
    if (previous_stake == previous_stakes_map.end()) {
      changes_map.emplace(std::move(current_stake));
      continue;
    }

    // Previous stakes contains validator address from current stakes -> substitute the stakes
    changes_map[current_stake.first] = PillarBlock::ValidatorStakeChange{
        current_stake.first, current_stake.second.stake - previous_stake->second.stake};

    // Delete item from previous_stakes - based on left stakes we know which delegators undelegated all tokens
    previous_stakes_map.erase(previous_stake);
  }

  // All previous stakes that were not deleted are delegators who undelegated all of their tokens between current and
  // previous pillar block. Add these stakes as negative numbers into changes
  for (auto& previous_stake_left : previous_stakes_map) {
    previous_stake_left.second.stake *= -1;
    changes_map.emplace(std::move(previous_stake_left));
  }

  // Transform ordered map of changes to vector
  std::vector<PillarBlock::ValidatorStakeChange> changes;
  changes.reserve(changes_map.size());
  std::transform(changes_map.begin(), changes_map.end(), std::back_inserter(changes),
                 [](auto& stake_change) { return std::move(stake_change.second); });

  return changes;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
