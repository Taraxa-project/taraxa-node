#pragma once

#include "dag/dag_block.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap::v4 {

struct DagSyncPacket {
  DagSyncPacket(const dev::RLP& packet_rlp) {
    if (constexpr size_t required_size = 4; packet_rlp.itemCount() != required_size) {
      throw InvalidRlpItemsCountException("DagSyncPacket", packet_rlp.itemCount(), required_size);
    }

    auto it = packet_rlp.begin();
    request_period = (*it++).toInt<PbftPeriod>();
    response_period = (*it++).toInt<PbftPeriod>();

    const auto trx_count = (*it).itemCount();
    transactions.reserve(trx_count);

    for (const auto tx_rlp : (*it++)) {
      try {
        auto trx = std::make_shared<Transaction>(tx_rlp);
        transactions.emplace(trx->getHash(), std::move(trx));
      } catch (const Transaction::InvalidTransaction& e) {
        throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
      }
    }

    dag_blocks.reserve((*it).itemCount());
    for (const auto block_rlp : *it) {
      dag_blocks.emplace_back(DagBlock{block_rlp});
    }
  };

  PbftPeriod request_period;
  PbftPeriod response_period;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> transactions;
  std::vector<DagBlock> dag_blocks;
};

}  // namespace taraxa::network::tarcap::v4
