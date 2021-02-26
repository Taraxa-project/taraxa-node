#ifndef ALETH_LIBETHEREUM_CHAINDB_H_
#define ALETH_LIBETHEREUM_CHAINDB_H_

#include <libdevcore/Exceptions.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>

#include "LogFilter.h"
#include "transaction_manager/transaction.hpp"
#include "types.h"

namespace taraxa::final_chain {

struct ChainDB {
  virtual ~ChainDB() {}

  virtual BlockNumber last_block_number() const = 0;
  virtual std::optional<BlockHeaderWithTransactions> blockHeaderWithTransactions(BlockNumber n,
                                                                                 bool full_transactions) const = 0;
  virtual std::optional<BlockHeaderWithTransactions> blockHeaderWithTransactions(h256 const& _hash,
                                                                                 bool full_transactions) const = 0;
  virtual LocalisedLogEntries logs(LogFilter const& _filter) const = 0;
  virtual std::optional<LocalisedTransaction> localisedTransaction(unsigned _i, BlockNumber n) const = 0;
  virtual std::optional<LocalisedTransaction> localisedTransaction(h256 const& _blockHash, unsigned _i) const = 0;
  virtual std::optional<LocalisedTransaction> localisedTransaction(h256 const& _transactionHash) const = 0;
  virtual std::optional<LocalisedTransactionReceipt> localisedTransactionReceipt(
      h256 const& _transactionHash) const = 0;
  virtual unsigned transactionCount(BlockNumber n) const = 0;
  virtual unsigned transactionCount(h256 const& _blockHash) const = 0;
};

};  // namespace taraxa::final_chain

#endif  // ALETH_LIBETHEREUM_CHAINDB_H_
