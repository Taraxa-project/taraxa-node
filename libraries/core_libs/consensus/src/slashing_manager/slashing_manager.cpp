#include "slashing_manager/slashing_manager.hpp"

#include "common/encoding_solidity.hpp"
#include "common/types.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

const auto kContractAddress = addr_t("0x00000000000000000000000000000000000000EE");

SlashingManager::SlashingManager(std::shared_ptr<FinalChain> final_chain,
                                 std::shared_ptr<TransactionManager> trx_manager, std::shared_ptr<GasPricer> gas_pricer,
                                 const FullNodeConfig &config, secret_t node_sk)
    : final_chain_(std::move(final_chain)),
      trx_manager_(std::move(trx_manager)),
      gas_pricer_(std::move(gas_pricer)),
      double_voting_proofs_(1000, 100),
      kConfig(config),
      kAddress(toAddress(node_sk)),
      kPrivateKey(std::move(node_sk)) {}

bool SlashingManager::submitDoubleVotingProof(const std::shared_ptr<PbftVote> &vote_a,
                                              const std::shared_ptr<PbftVote> &vote_b) {
  if (!kConfig.report_malicious_behaviour) {
    return false;
  }

  if (vote_a->getPeriod() < kConfig.genesis.state.hardforks.magnolia_hf.block_num) {
    return false;
  }

  if (vote_a->getPeriod() != vote_b->getPeriod() || vote_a->getRound() != vote_b->getRound() ||
      vote_a->getStep() != vote_b->getStep()) {
    return false;
  }

  // Create votes combination hash
  dev::RLPStream hash_rlp(2);
  if (vote_a->getHash() < vote_b->getHash()) {
    hash_rlp << vote_a->getHash();
    hash_rlp << vote_b->getHash();
  } else {
    hash_rlp << vote_b->getHash();
    hash_rlp << vote_a->getHash();
  }
  const auto hash_bytes = hash_rlp.invalidate();
  const auto hash = dev::sha3(hash_bytes);

  // Check if this proof wasn't already processed
  if (double_voting_proofs_.contains(hash)) {
    return false;
  }

  // Check the balance
  const auto account = final_chain_->get_account(kAddress).value_or(taraxa::state_api::ZeroAccount);
  if (account.balance == 0) {
    return false;
  }

  auto input =
      util::EncodingSolidity::packFunctionCall("commitDoubleVotingProof(bytes,bytes)", vote_a->rlp(), vote_b->rlp());
  const auto trx = std::make_shared<Transaction>(account.nonce, 0, gas_pricer_->bid(), 100000, std::move(input),
                                                 kPrivateKey, kContractAddress, kConfig.genesis.chain_id);

  if (trx_manager_->insertTransaction(trx).first) {
    double_voting_proofs_.insert(hash);
    return true;
  }

  return false;
}
}  // namespace taraxa