#pragma once

#include "QueryObject.h"
#include "dag/dag_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "network/network.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/gas_pricer.hpp"
#include "transaction/transaction_manager.hpp"

namespace graphql::taraxa {
class Query {
 public:
  explicit Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
                 std::shared_ptr<::taraxa::DagManager> dag_manager, std::shared_ptr<::taraxa::PbftManager> pbft_manager,
                 std::shared_ptr<::taraxa::TransactionManager> transaction_manager,
                 std::shared_ptr<::taraxa::DbStorage> db, std::shared_ptr<::taraxa::GasPricer> gas_pricer,
                 std::weak_ptr<::taraxa::Network> network, uint64_t chain_id) noexcept;

  std::shared_ptr<object::Block> getBlock(std::optional<response::Value>&& numberArg,
                                          std::optional<response::Value>&& hashArg) const;
  std::vector<std::shared_ptr<object::Block>> getBlocks(response::Value&& fromArg,
                                                        std::optional<response::Value>&& toArg) const;
  std::shared_ptr<object::Transaction> getTransaction(response::Value&& hashArg) const;
  std::shared_ptr<object::Account> getAccount(response::Value&& addressArg,
                                              std::optional<response::Value>&& blockArg) const;
  response::Value getGasPrice() const;
  std::shared_ptr<object::SyncState> getSyncing() const;
  response::Value getChainID() const;
  std::shared_ptr<object::DagBlock> getDagBlock(std::optional<response::Value>&& hashArg) const;
  std::vector<std::shared_ptr<object::DagBlock>> getDagBlocks(std::optional<response::Value>&& dagLevelArg,
                                                              std::optional<int>&& countArg,
                                                              std::optional<bool>&& reverseArg) const;
  std::shared_ptr<object::CurrentState> getNodeState() const;

 private:
  // TODO: use pagination limit for all "list" queries
  static constexpr size_t kMaxPropagationLimit{100};

  std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain_;
  std::shared_ptr<::taraxa::DagManager> dag_manager_;
  std::shared_ptr<::taraxa::PbftManager> pbft_manager_;
  std::shared_ptr<::taraxa::TransactionManager> transaction_manager_;
  std::shared_ptr<::taraxa::DbStorage> db_;
  std::shared_ptr<::taraxa::GasPricer> gas_pricer_;
  std::weak_ptr<::taraxa::Network> network_;
  const uint64_t kChainId;
};

}  // namespace graphql::taraxa