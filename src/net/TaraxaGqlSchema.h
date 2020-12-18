// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef TARAXAGQLSCHEMA_H
#define TARAXAGQLSCHEMA_H

#include <memory>
#include <string>
#include <vector>

#include "graphqlservice/GraphQLParse.h"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"

namespace graphql {
namespace taraxa {

struct BlockFilterCriteria;
struct CallData;
struct FilterCriteria;

struct BlockFilterCriteria {
  std::optional<std::vector<response::Value>> addresses;
  std::optional<std::vector<std::vector<response::Value>>> topics;
};

struct CallData {
  std::optional<response::Value> from;
  std::optional<response::Value> to;
  std::optional<response::Value> gas;
  std::optional<response::Value> gasPrice;
  std::optional<response::Value> value;
  std::optional<response::Value> data;
};

struct FilterCriteria {
  std::optional<response::Value> fromBlock;
  std::optional<response::Value> toBlock;
  std::optional<std::vector<response::Value>> addresses;
  std::optional<std::vector<std::vector<response::Value>>> topics;
};

namespace object {

class Account;
class Log;
class Transaction;
class Block;
class CallResult;
class SyncState;
class Pending;
class Query;
class Mutation;

class Account : public service::Object {
 protected:
  explicit Account();

 public:
  virtual service::FieldResult<response::Value> getAddress(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getBalance(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getTransactionCount(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getCode(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getStorage(service::FieldParams&& params,
                                                           response::Value&& slotArg) const;

 private:
  std::future<service::ResolverResult> resolveAddress(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveBalance(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactionCount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCode(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveStorage(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Log : public service::Object {
 protected:
  explicit Log();

 public:
  virtual service::FieldResult<response::IntType> getIndex(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getAccount(service::FieldParams&& params,
                                                                    std::optional<response::Value>&& blockArg) const;
  virtual service::FieldResult<std::vector<response::Value>> getTopics(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getData(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Transaction>> getTransaction(service::FieldParams&& params) const;

 private:
  std::future<service::ResolverResult> resolveIndex(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveAccount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTopics(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveData(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransaction(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Transaction : public service::Object {
 protected:
  explicit Transaction();

 public:
  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getNonce(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::IntType>> getIndex(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getFrom(service::FieldParams&& params,
                                                                 std::optional<response::Value>&& blockArg) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getTo(service::FieldParams&& params,
                                                               std::optional<response::Value>&& blockArg) const;
  virtual service::FieldResult<response::Value> getValue(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getGasPrice(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getGas(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getInputData(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Block>> getBlock(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::Value>> getStatus(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::Value>> getGasUsed(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::Value>> getCumulativeGasUsed(
      service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getCreatedContract(
      service::FieldParams&& params, std::optional<response::Value>&& blockArg) const;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<Log>>>> getLogs(
      service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getR(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getS(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getV(service::FieldParams&& params) const;

 private:
  std::future<service::ResolverResult> resolveHash(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveNonce(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveIndex(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveFrom(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTo(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveValue(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasPrice(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGas(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveInputData(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveBlock(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveStatus(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasUsed(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCumulativeGasUsed(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCreatedContract(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveLogs(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveR(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveS(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveV(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Block : public service::Object {
 protected:
  explicit Block();

 public:
  virtual service::FieldResult<response::Value> getNumber(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getHash(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Block>> getParent(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getNonce(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getTransactionsRoot(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::IntType>> getTransactionCount(
      service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getStateRoot(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getReceiptsRoot(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getMiner(service::FieldParams&& params,
                                                                  std::optional<response::Value>&& blockArg) const;
  virtual service::FieldResult<response::Value> getExtraData(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getGasLimit(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getGasUsed(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getTimestamp(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getLogsBloom(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getMixHash(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getDifficulty(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getTotalDifficulty(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::IntType>> getOmmerCount(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<Block>>>> getOmmers(
      service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Block>> getOmmerAt(service::FieldParams&& params,
                                                                  response::IntType&& indexArg) const;
  virtual service::FieldResult<response::Value> getOmmerHash(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<Transaction>>>> getTransactions(
      service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Transaction>> getTransactionAt(service::FieldParams&& params,
                                                                              response::IntType&& indexArg) const;
  virtual service::FieldResult<std::vector<std::shared_ptr<Log>>> getLogs(service::FieldParams&& params,
                                                                          BlockFilterCriteria&& filterArg) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getAccount(service::FieldParams&& params,
                                                                    response::Value&& addressArg) const;
  virtual service::FieldResult<std::shared_ptr<CallResult>> getCall(service::FieldParams&& params,
                                                                    CallData&& dataArg) const;
  virtual service::FieldResult<response::Value> getEstimateGas(service::FieldParams&& params, CallData&& dataArg) const;

 private:
  std::future<service::ResolverResult> resolveNumber(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveHash(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveParent(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveNonce(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactionsRoot(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactionCount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveStateRoot(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveReceiptsRoot(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveMiner(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveExtraData(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasLimit(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasUsed(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTimestamp(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveLogsBloom(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveMixHash(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveDifficulty(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTotalDifficulty(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveOmmerCount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveOmmers(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveOmmerAt(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveOmmerHash(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactions(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactionAt(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveLogs(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveAccount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCall(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveEstimateGas(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class CallResult : public service::Object {
 protected:
  explicit CallResult();

 public:
  virtual service::FieldResult<response::Value> getData(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getGasUsed(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getStatus(service::FieldParams&& params) const;

 private:
  std::future<service::ResolverResult> resolveData(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasUsed(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveStatus(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class SyncState : public service::Object {
 protected:
  explicit SyncState();

 public:
  virtual service::FieldResult<response::Value> getStartingBlock(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getCurrentBlock(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getHighestBlock(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::Value>> getPulledStates(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<response::Value>> getKnownStates(service::FieldParams&& params) const;

 private:
  std::future<service::ResolverResult> resolveStartingBlock(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCurrentBlock(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveHighestBlock(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolvePulledStates(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveKnownStates(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Pending : public service::Object {
 protected:
  explicit Pending();

 public:
  virtual service::FieldResult<response::IntType> getTransactionCount(service::FieldParams&& params) const;
  virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<Transaction>>>> getTransactions(
      service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Account>> getAccount(service::FieldParams&& params,
                                                                    response::Value&& addressArg) const;
  virtual service::FieldResult<std::shared_ptr<CallResult>> getCall(service::FieldParams&& params,
                                                                    CallData&& dataArg) const;
  virtual service::FieldResult<response::Value> getEstimateGas(service::FieldParams&& params, CallData&& dataArg) const;

 private:
  std::future<service::ResolverResult> resolveTransactionCount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransactions(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveAccount(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveCall(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveEstimateGas(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Query : public service::Object {
 protected:
  explicit Query();

 public:
  virtual service::FieldResult<std::shared_ptr<Block>> getBlock(service::FieldParams&& params,
                                                                std::optional<response::Value>&& numberArg,
                                                                std::optional<response::Value>&& hashArg) const;
  virtual service::FieldResult<std::vector<std::shared_ptr<Block>>> getBlocks(
      service::FieldParams&& params, response::Value&& fromArg, std::optional<response::Value>&& toArg) const;
  virtual service::FieldResult<std::shared_ptr<Pending>> getPending(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<Transaction>> getTransaction(service::FieldParams&& params,
                                                                            response::Value&& hashArg) const;
  virtual service::FieldResult<std::vector<std::shared_ptr<Log>>> getLogs(service::FieldParams&& params,
                                                                          FilterCriteria&& filterArg) const;
  virtual service::FieldResult<response::Value> getGasPrice(service::FieldParams&& params) const;
  virtual service::FieldResult<response::IntType> getProtocolVersion(service::FieldParams&& params) const;
  virtual service::FieldResult<std::shared_ptr<SyncState>> getSyncing(service::FieldParams&& params) const;
  virtual service::FieldResult<response::Value> getChainID(service::FieldParams&& params) const;

 private:
  std::future<service::ResolverResult> resolveBlock(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveBlocks(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolvePending(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveTransaction(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveLogs(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveGasPrice(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveProtocolVersion(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveSyncing(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolveChainID(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolve_schema(service::ResolverParams&& params);
  std::future<service::ResolverResult> resolve_type(service::ResolverParams&& params);

  std::shared_ptr<schema::Schema> _schema;
};

class Mutation : public service::Object {
 protected:
  explicit Mutation();

 public:
  virtual service::FieldResult<response::Value> applySendRawTransaction(service::FieldParams&& params,
                                                                        response::Value&& dataArg) const;

 private:
  std::future<service::ResolverResult> resolveSendRawTransaction(service::ResolverParams&& params);

  std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

} /* namespace object */

class Operations : public service::Request {
 public:
  explicit Operations(std::shared_ptr<object::Query> query, std::shared_ptr<object::Mutation> mutation);

 private:
  std::shared_ptr<object::Query> _query;
  std::shared_ptr<object::Mutation> _mutation;
};

std::shared_ptr<schema::Schema> GetSchema();

} /* namespace taraxa */
} /* namespace graphql */

#endif  // TARAXAGQLSCHEMA_H
