#pragma once

#include "final_chain/final_chain.hpp"
#include "transaction/gas_pricer.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {
class SlashingManager {
 public:
  SlashingManager(const FullNodeConfig &config, std::shared_ptr<final_chain::FinalChain> final_chain,
                  std::shared_ptr<TransactionManager> trx_manager, std::shared_ptr<GasPricer> gas_pricer);
  SlashingManager(const SlashingManager &) = delete;
  SlashingManager(SlashingManager &&) = delete;
  SlashingManager &operator=(const SlashingManager &) = delete;
  SlashingManager &operator=(SlashingManager &&) = delete;

  bool submitDoubleVotingProof(const std::shared_ptr<PbftVote> &vote_a, const std::shared_ptr<PbftVote> &vote_b,
                               const WalletConfig &wallet);

 private:
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<TransactionManager> trx_manager_;
  std::shared_ptr<GasPricer> gas_pricer_;

  // Already processed double voting proofs
  ExpirationCache<dev::h256> double_voting_proofs_;

  const FullNodeConfig &kConfig;
};
}  // namespace taraxa