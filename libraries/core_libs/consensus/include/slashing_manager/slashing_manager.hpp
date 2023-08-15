#pragma once

#include "final_chain/final_chain.hpp"
#include "transaction/gas_pricer.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {
class SlashingManager {
 public:
  SlashingManager(std::shared_ptr<FinalChain> final_chain, std::shared_ptr<TransactionManager> trx_manager,
                  std::shared_ptr<GasPricer> gas_pricer, uint64_t chain_id, secret_t node_sk);
  SlashingManager(const SlashingManager &) = delete;
  SlashingManager(SlashingManager &&) = delete;
  SlashingManager &operator=(const SlashingManager &) = delete;
  SlashingManager &operator=(SlashingManager &&) = delete;

  bool submitDoubleVotingProof(const std::shared_ptr<Vote> &vote_a, const std::shared_ptr<Vote> &vote_b) const;

 private:
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<TransactionManager> trx_manager_;
  std::shared_ptr<GasPricer> gas_pricer_;
  const uint64_t kChainId;
  const addr_t kAddress;
  const secret_t kPrivateKey;
};
}  // namespace taraxa