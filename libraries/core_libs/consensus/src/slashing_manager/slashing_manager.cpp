#include "slashing_manager/slashing_manager.hpp"

#include "common/types.hpp"
#include "final_chain/contract_interface.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

const auto kContractAddress = addr_t("0x00000000000000000000000000000000000000EE");

SlashingManager::SlashingManager(std::shared_ptr<FinalChain> final_chain,
                                 std::shared_ptr<TransactionManager> trx_manager, std::shared_ptr<GasPricer> gas_pricer,
                                 uint64_t chain_id, secret_t node_sk)
    : final_chain_(std::move(final_chain)),
      trx_manager_(std::move(trx_manager)),
      gas_pricer_(std::move(gas_pricer)),
      double_voting_proofs_(10000, 1000),
      kChainId(chain_id),
      kAddress(toAddress(node_sk)),
      kPrivateKey(std::move(node_sk)) {}

bool SlashingManager::submitDoubleVotingProof(const std::shared_ptr<Vote> &vote_a,
                                              const std::shared_ptr<Vote> &vote_b) {
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

  auto input = final_chain::ContractInterface::packFunctionCall("commitDoubleVotingProof(bytes,bytes)", vote_a->rlp(),
                                                                vote_b->rlp());

  // TODO we need to calculate gas for this transaction and hardcode it
  // CommitDoubleVotingProofGas uint64 = 20000
  const auto trx = std::make_shared<Transaction>(account.nonce, 0, gas_pricer_->bid(), 1000000, std::move(input),
                                                 kPrivateKey, kContractAddress, kChainId);

  if (trx_manager_->insertTransaction(trx).first) {
    double_voting_proofs_.insert(hash);
    return true;
  }

  return false;
}
}  // namespace taraxa