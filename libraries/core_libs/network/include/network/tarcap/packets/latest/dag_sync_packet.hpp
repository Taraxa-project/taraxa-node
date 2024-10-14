#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct DagSyncPacket {
  DagSyncPacket() = default;
  DagSyncPacket(const DagSyncPacket&) = default;
  DagSyncPacket(DagSyncPacket&&) = default;
  DagSyncPacket& operator=(const DagSyncPacket&) = default;
  DagSyncPacket& operator=(DagSyncPacket&&) = default;
  DagSyncPacket(PbftPeriod request_period, PbftPeriod response_period,
                std::vector<std::shared_ptr<Transaction>>&& transactions,
                std::vector<std::shared_ptr<DagBlock>>&& dag_blocks)
      : request_period(request_period),
        response_period(response_period),
        transactions(std::move(transactions)),
        dag_blocks(std::move(dag_blocks)) {}
  DagSyncPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<DagSyncPacket>(packet_rlp); }

  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  PbftPeriod request_period;
  PbftPeriod response_period;
  std::vector<std::shared_ptr<Transaction>> transactions;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;

  RLP_FIELDS_DEFINE_INPLACE(request_period, response_period, transactions, dag_blocks)
};

}  // namespace taraxa::network::tarcap
