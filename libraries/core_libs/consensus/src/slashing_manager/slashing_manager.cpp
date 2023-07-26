#include "slashing_manager/slashing_manager.hpp"

#include "common/types.hpp"
#include "final_chain/contract_interface.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

const auto kContractAddress = addr_t("0x00000000000000000000000000000000000000FE");

SlashingManager::SlashingManager(std::shared_ptr<FinalChain> final_chain,
                                 std::shared_ptr<TransactionManager> trx_manager, std::shared_ptr<GasPricer> gas_pricer,
                                 uint64_t chain_id, secret_t node_sk)
    : final_chain_(std::move(final_chain)),
      trx_manager_(std::move(trx_manager)),
      gas_pricer_(std::move(gas_pricer)),
      kChainId(chain_id),
      kAddress(toAddress(node_sk)),
      kPrivateKey(std::move(node_sk)) {}

bool SlashingManager::submit_double_voting(const std::shared_ptr<Vote> &vote_a,
                                           const std::shared_ptr<Vote> &vote_b) const {
  // First we need to check the balance
  const auto account = final_chain_->get_account(kAddress).value_or(taraxa::state_api::ZeroAccount);
  if (account.balance == 0) {
    return false;
  }

  // function commitDoubleVotingProof(
  //     bytes memory vote1,
  //     bytes memory vote2
  // ) external;

  auto input = final_chain::ContractInterface::packFunctionCall("commitDoubleVotingProof(bytes,bytes)", vote_a->rlp(),
                                                                vote_b->rlp());

  // TODO we need to calculate gas for this transaction and hardcode it
  // CommitDoubleVotingProofGas uint64 = 20000
  const auto trx = std::make_shared<Transaction>(account.nonce, 0, gas_pricer_->bid(), 1000000, std::move(input),
                                                 kPrivateKey, kContractAddress, kChainId);
  return trx_manager_->insertTransaction(trx).first;
}
}  // namespace taraxa