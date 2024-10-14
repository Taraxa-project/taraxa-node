#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct DagBlockPacket {
  DagBlockPacket() = default;
  DagBlockPacket(const DagBlockPacket&) = default;
  DagBlockPacket(DagBlockPacket&&) = default;
  DagBlockPacket& operator=(const DagBlockPacket&) = default;
  DagBlockPacket& operator=(DagBlockPacket&&) = default;
  // TODO[2868]: optimize args
  DagBlockPacket(const std::vector<std::shared_ptr<Transaction>>& transactions, const DagBlock& dag_block)
      : transactions(transactions), dag_block(dag_block) {}

  DagBlockPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<DagBlockPacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  std::vector<std::shared_ptr<Transaction>> transactions;
  DagBlock dag_block;

  RLP_FIELDS_DEFINE_INPLACE(transactions, dag_block)
};

}  // namespace taraxa::network::tarcap
