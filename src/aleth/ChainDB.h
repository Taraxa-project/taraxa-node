#ifndef ALETH_LIBETHEREUM_CHAINDB_H_
#define ALETH_LIBETHEREUM_CHAINDB_H_

#include <libdevcore/Exceptions.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>

#include "BlockDetails.h"
#include "BlockHeader.h"
#include "Common.h"
#include "LocalizedTransaction.h"
#include "LogFilter.h"
#include "TransactionReceipt.h"
#include "db.h"

namespace taraxa::aleth {

struct ChainDB {
  virtual ~ChainDB() {}

  virtual BlockNumber last_block_number() const = 0;
  virtual bool isKnown(BlockNumber _block) const = 0;
  virtual bool isKnown(h256 const& _hash) const = 0;
  virtual BlockHeader blockHeader(BlockNumber n) const = 0;
  virtual BlockHeader blockHeader(h256 const& _hash) const = 0;
  virtual BlockDetails blockDetails(BlockNumber n) const = 0;
  virtual BlockDetails blockDetails(h256 const& _hash) const = 0;
  virtual TransactionReceipt transactionReceipt(h256 const& _blockHash, unsigned _i) const = 0;
  virtual TransactionReceipt transactionReceipt(h256 const& _transactionHash) const = 0;
  virtual bool isKnownTransaction(h256 const& _transactionHash) const = 0;
  virtual bool isKnownTransaction(unsigned _i, BlockNumber n) const = 0;
  virtual bool isKnownTransaction(h256 const& _blockHash, unsigned _i) const = 0;
  virtual LocalisedLogEntries logs(LogFilter const& _filter) const = 0;
  virtual Transaction transaction(h256 _transactionHash) const = 0;
  virtual Transaction transaction(h256 _blockHash, unsigned _i) const = 0;
  virtual LocalisedTransaction localisedTransaction(unsigned _i, BlockNumber n) const = 0;
  virtual LocalisedTransaction localisedTransaction(h256 const& _blockHash, unsigned _i) const = 0;
  virtual LocalisedTransaction localisedTransaction(h256 const& _transactionHash) const = 0;
  virtual LocalisedTransactionReceipt localisedTransactionReceipt(h256 const& _transactionHash) const = 0;
  virtual Transactions transactions(BlockNumber n) const = 0;
  virtual Transactions transactions(h256 _blockHash) const = 0;
  virtual h256s transactionHashes(BlockNumber n) const = 0;
  virtual h256s transactionHashes(h256 const& _hash) const = 0;
  virtual unsigned transactionCount(BlockNumber n) const = 0;
  virtual unsigned transactionCount(h256 const& _blockHash) const = 0;
};

};  // namespace taraxa::aleth

#endif  // ALETH_LIBETHEREUM_CHAINDB_H_
