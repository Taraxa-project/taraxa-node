#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network {

// TODO: create new version of this packet without manual parsing
struct DagBlockPacket {
  DagBlockPacket(const dev::RLP& packet_rlp) {
    constexpr size_t required_size = 2;
    // Only one dag block can be received
    if (packet_rlp.itemCount() != required_size) {
      throw InvalidRlpItemsCountException("DagBlockPacket", packet_rlp.itemCount(), required_size);
    }

    dev::RLP dag_rlp;

    // TODO: bad rlp form - we should not check itemsCount here...
    if (packet_rlp.itemCount() == 2) {
      const auto trx_count = packet_rlp[0].itemCount();
      transactions.reserve(trx_count);

      for (const auto tx_rlp : packet_rlp[0]) {
        try {
          auto trx = std::make_shared<Transaction>(tx_rlp);
          transactions.emplace(trx->getHash(), std::move(trx));
        } catch (const Transaction::InvalidTransaction& e) {
          throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
        }
      }
      dag_rlp = packet_rlp[1];
    } else {
      dag_rlp = packet_rlp;
    }

    dag_block = DagBlock(dag_rlp);
  };

  // TODO: make this a vector for automatic encoding/decoding...
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> transactions;
  DagBlock dag_block;

  // RLP_FIELDS_DEFINE_INPLACE(transactions, dag_block)
};

}  // namespace taraxa::network
