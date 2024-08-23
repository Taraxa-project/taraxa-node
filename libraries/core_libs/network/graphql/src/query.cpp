#include "graphql/query.hpp"

#include <libdevcore/CommonJS.h>

#include "graphql/account.hpp"
#include "graphql/block.hpp"
#include "graphql/log.hpp"
#include "graphql/sync_state.hpp"
#include "graphql/transaction.hpp"
#include "graphql/types/current_state.hpp"
#include "graphql/types/dag_block.hpp"

using namespace std::literals;

namespace graphql::taraxa {

Query::Query(std::shared_ptr<::taraxa::final_chain::FinalChain> final_chain,
             std::shared_ptr<::taraxa::DagManager> dag_manager, std::shared_ptr<::taraxa::PbftManager> pbft_manager,
             std::shared_ptr<::taraxa::TransactionManager> transaction_manager, std::shared_ptr<::taraxa::DbStorage> db,
             std::shared_ptr<::taraxa::GasPricer> gas_pricer, std::weak_ptr<::taraxa::Network> network,
             uint64_t chain_id) noexcept
    : final_chain_(std::move(final_chain)),
      dag_manager_(std::move(dag_manager)),
      pbft_manager_(std::move(pbft_manager)),
      transaction_manager_(std::move(transaction_manager)),
      db_(std::move(db)),
      gas_pricer_(std::move(gas_pricer)),
      network_(std::move(network)),
      kChainId(chain_id) {
  get_block_by_num_ = [&](::taraxa::EthBlockNumber num) {
    return getBlock(response::Value(static_cast<int>(num)), std::nullopt);
  };
}

std::shared_ptr<object::Block> Query::getBlock(std::optional<response::Value>&& number,
                                               std::optional<response::Value>&& hash) const {
  std::optional<::taraxa::EthBlockNumber> block_number;
  if (number) {
    block_number = number->get<int>();
    if (const auto last_block_number = final_chain_->lastBlockNumber(); last_block_number < block_number) {
      return nullptr;
    }
  }
  if (hash) {
    block_number = final_chain_->blockNumber(dev::h256(hash->get<std::string>()));
    if (!block_number) {
      return nullptr;
    }
  }
  auto block_header = final_chain_->blockHeader(block_number);
  if (!block_header) {
    return nullptr;
  }

  // Special case for genesis
  if (block_number == 0) [[unlikely]] {
    return std::make_shared<object::Block>(std::make_shared<Block>(
        final_chain_, transaction_manager_, get_block_by_num_, ::taraxa::blk_hash_t(), block_header));
  }

  auto pbft_block = db_->getPbftBlock(block_header->number);
  if (!pbft_block) {
    // shouldn't be possible
    return nullptr;
  }
  return std::make_shared<object::Block>(std::make_shared<Block>(final_chain_, transaction_manager_, get_block_by_num_,
                                                                 pbft_block->getBlockHash(), block_header));
}

std::vector<std::shared_ptr<object::Block>> Query::getBlocks(response::Value&& fromArg,
                                                             std::optional<response::Value>&& toArg) const {
  std::vector<std::shared_ptr<object::Block>> blocks;

  int start_block_num = fromArg.get<int>();
  int end_block_num = toArg ? toArg->get<int>() : (start_block_num + Query::kMaxPropagationLimit);

  // Incase of reverse order of blocks
  if (start_block_num > end_block_num) {
    auto tmp = start_block_num;
    start_block_num = end_block_num;
    end_block_num = tmp;
  }

  if (end_block_num - start_block_num > static_cast<int>(Query::kMaxPropagationLimit)) {
    end_block_num = start_block_num + Query::kMaxPropagationLimit;
  }

  const int last_block_number = final_chain_->lastBlockNumber();
  if (start_block_num > last_block_number) {
    return blocks;
  } else if (end_block_num > last_block_number) {
    end_block_num = last_block_number;
  }

  blocks.reserve(end_block_num - start_block_num);

  for (int block_num = start_block_num; block_num <= end_block_num; block_num++) {
    blocks.emplace_back(getBlock(response::Value(block_num), std::nullopt));
  }

  return blocks;
}

std::shared_ptr<object::Transaction> Query::getTransaction(response::Value&& hashArg) const {
  if (auto transaction = transaction_manager_->getTransaction(::taraxa::trx_hash_t(hashArg.get<std::string>()))) {
    return std::make_shared<object::Transaction>(
        std::make_shared<Transaction>(final_chain_, transaction_manager_, get_block_by_num_, std::move(transaction)));
  }
  return nullptr;
}

std::shared_ptr<object::Account> Query::getAccount(response::Value&& addressArg,
                                                   std::optional<response::Value>&& blockArg) const {
  const auto address = ::taraxa::addr_t(addressArg.get<std::string>());
  if (blockArg) {
    return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, address, blockArg->get<int>()));
  } else {
    return std::make_shared<object::Account>(std::make_shared<Account>(final_chain_, address));
  }
}

response::Value Query::getGasPrice() const { return response::Value(dev::toJS(gas_pricer_->bid())); }

std::shared_ptr<object::SyncState> Query::getSyncing() const {
  return std::make_shared<object::SyncState>(std::make_shared<SyncState>(final_chain_, network_));
}

response::Value Query::getChainID() const { return response::Value(dev::toJS(kChainId)); }

std::shared_ptr<object::DagBlock> Query::getDagBlock(std::optional<response::Value>&& hashArg) const {
  std::shared_ptr<::taraxa::DagBlock> taraxa_dag_block = nullptr;

  if (hashArg) {
    if (const auto hash = ::taraxa::blk_hash_t(hashArg->get<response::StringType>());
        hash != ::taraxa::kNullBlockHash) {
      taraxa_dag_block = dag_manager_->getDagBlock(hash);
    }
  } else {
    auto dag_blocks = db_->getDagBlocksAtLevel(dag_manager_->getMaxLevel(), 1);

    if (dag_blocks.size() > 0) {
      taraxa_dag_block = dag_blocks.front();
    }
  }
  if (taraxa_dag_block) {
    return std::make_shared<object::DagBlock>(std::make_shared<DagBlock>(
        std::move(taraxa_dag_block), final_chain_, pbft_manager_, transaction_manager_, get_block_by_num_));
  }
  return nullptr;
}

std::vector<std::shared_ptr<object::DagBlock>> Query::getPeriodDagBlocks(
    std::optional<response::Value>&& periodArg) const {
  std::vector<std::shared_ptr<object::DagBlock>> blocks;
  uint32_t period;
  if (periodArg) {
    period = periodArg->get<int>();
  } else {
    period = final_chain_->lastBlockNumber();
  }
  auto dag_blocks = db_->getFinalizedDagBlockByPeriod(period);
  if (dag_blocks.size()) {
    blocks.reserve(dag_blocks.size());
    for (auto block : dag_blocks) {
      blocks.emplace_back(std::make_shared<object::DagBlock>(std::make_shared<DagBlock>(
          std::move(block), final_chain_, pbft_manager_, transaction_manager_, get_block_by_num_)));
    }
  }
  return blocks;
}

std::vector<std::shared_ptr<object::DagBlock>> Query::getDagBlocks(std::optional<response::Value>&& dagLevelArg,
                                                                   std::optional<int>&& countArg,
                                                                   std::optional<bool>&& reverseArg) const {
  std::vector<std::shared_ptr<object::DagBlock>> dag_blocks_result;
  ::taraxa::level_t act_dag_level = dag_manager_->getMaxLevel();

  if (dagLevelArg) {
    act_dag_level = dagLevelArg->get<int>();
    if (act_dag_level < 0 || act_dag_level > dag_manager_->getMaxLevel()) {
      return dag_blocks_result;
    }
  }

  auto addDagBlocks = [final_chain = final_chain_, pbft_manager = pbft_manager_,
                       transaction_manager = transaction_manager_, get_block_by_num = get_block_by_num_](
                          auto taraxa_dag_blocks, auto& result_dag_blocks) -> size_t {
    for (auto& dag_block : taraxa_dag_blocks) {
      result_dag_blocks.emplace_back(std::make_shared<object::DagBlock>(std::make_shared<DagBlock>(
          std::move(dag_block), final_chain, pbft_manager, transaction_manager, get_block_by_num)));
    }

    return taraxa_dag_blocks.size();
  };

  auto act_count = addDagBlocks(db_->getDagBlocksAtLevel(act_dag_level, 1), dag_blocks_result);

  if (!countArg) {
    return dag_blocks_result;
  }

  auto count = std::min(static_cast<size_t>(countArg.value()), Query::kMaxPropagationLimit);
  bool reverse_flag = reverseArg ? reverseArg.value() : false;

  while (act_count < count && act_dag_level <= dag_manager_->getMaxLevel()) {
    if (!reverse_flag) {
      act_dag_level++;
    } else if (act_dag_level > 0) {
      act_dag_level--;
    } else {
      return dag_blocks_result;
    }

    act_count += addDagBlocks(db_->getDagBlocksAtLevel(act_dag_level, 1), dag_blocks_result);
  }

  return dag_blocks_result;
}

std::shared_ptr<object::CurrentState> Query::getNodeState() const {
  return std::make_shared<object::CurrentState>(std::make_shared<CurrentState>(final_chain_, dag_manager_));
}

}  // namespace graphql::taraxa