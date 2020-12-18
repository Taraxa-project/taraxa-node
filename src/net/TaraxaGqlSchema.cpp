// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "TaraxaGqlSchema.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include "graphqlservice/introspection/Introspection.h"

using namespace std::literals;

namespace graphql {
namespace service {

template <>
taraxa::BlockFilterCriteria ModifiedArgument<taraxa::BlockFilterCriteria>::convert(const response::Value& value) {
  auto valueAddresses =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable, service::TypeModifier::List>(
          "addresses", value);
  auto valueTopics =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable, service::TypeModifier::List,
                                                          service::TypeModifier::List>("topics", value);

  return {std::move(valueAddresses), std::move(valueTopics)};
}

template <>
taraxa::CallData ModifiedArgument<taraxa::CallData>::convert(const response::Value& value) {
  auto valueFrom = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("from", value);
  auto valueTo = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("to", value);
  auto valueGas = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("gas", value);
  auto valueGasPrice =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("gasPrice", value);
  auto valueValue =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("value", value);
  auto valueData = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("data", value);

  return {std::move(valueFrom),     std::move(valueTo),    std::move(valueGas),
          std::move(valueGasPrice), std::move(valueValue), std::move(valueData)};
}

template <>
taraxa::FilterCriteria ModifiedArgument<taraxa::FilterCriteria>::convert(const response::Value& value) {
  auto valueFromBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("fromBlock", value);
  auto valueToBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("toBlock", value);
  auto valueAddresses =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable, service::TypeModifier::List>(
          "addresses", value);
  auto valueTopics =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable, service::TypeModifier::List,
                                                          service::TypeModifier::List>("topics", value);

  return {std::move(valueFromBlock), std::move(valueToBlock), std::move(valueAddresses), std::move(valueTopics)};
}

} /* namespace service */

namespace taraxa {
namespace object {

Account::Account()
    : service::Object(
          {"Account"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(address)gql"sv,
            [this](service::ResolverParams&& params) { return resolveAddress(std::move(params)); }},
           {R"gql(balance)gql"sv,
            [this](service::ResolverParams&& params) { return resolveBalance(std::move(params)); }},
           {R"gql(code)gql"sv, [this](service::ResolverParams&& params) { return resolveCode(std::move(params)); }},
           {R"gql(storage)gql"sv,
            [this](service::ResolverParams&& params) { return resolveStorage(std::move(params)); }},
           {R"gql(transactionCount)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactionCount(std::move(params)); }}}) {}

service::FieldResult<response::Value> Account::getAddress(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Account::getAddress is not implemented)ex");
}

std::future<service::ResolverResult> Account::resolveAddress(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getAddress(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Account::getBalance(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Account::getBalance is not implemented)ex");
}

std::future<service::ResolverResult> Account::resolveBalance(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getBalance(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Account::getTransactionCount(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Account::getTransactionCount is not implemented)ex");
}

std::future<service::ResolverResult> Account::resolveTransactionCount(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactionCount(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Account::getCode(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Account::getCode is not implemented)ex");
}

std::future<service::ResolverResult> Account::resolveCode(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getCode(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Account::getStorage(service::FieldParams&&, response::Value&&) const {
  throw std::runtime_error(R"ex(Account::getStorage is not implemented)ex");
}

std::future<service::ResolverResult> Account::resolveStorage(service::ResolverParams&& params) {
  auto argSlot = service::ModifiedArgument<response::Value>::require("slot", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getStorage(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argSlot));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Account::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Account)gql"},
                                                                std::move(params));
}

Log::Log()
    : service::Object(
          {"Log"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(account)gql"sv,
            [this](service::ResolverParams&& params) { return resolveAccount(std::move(params)); }},
           {R"gql(data)gql"sv, [this](service::ResolverParams&& params) { return resolveData(std::move(params)); }},
           {R"gql(index)gql"sv, [this](service::ResolverParams&& params) { return resolveIndex(std::move(params)); }},
           {R"gql(topics)gql"sv, [this](service::ResolverParams&& params) { return resolveTopics(std::move(params)); }},
           {R"gql(transaction)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransaction(std::move(params)); }}}) {}

service::FieldResult<response::IntType> Log::getIndex(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Log::getIndex is not implemented)ex");
}

std::future<service::ResolverResult> Log::resolveIndex(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getIndex(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Log::getAccount(service::FieldParams&&,
                                                               std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Log::getAccount is not implemented)ex");
}

std::future<service::ResolverResult> Log::resolveAccount(service::ResolverParams&& params) {
  auto argBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("block", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getAccount(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argBlock));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::vector<response::Value>> Log::getTopics(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Log::getTopics is not implemented)ex");
}

std::future<service::ResolverResult> Log::resolveTopics(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTopics(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::List>(std::move(result),
                                                                                        std::move(params));
}

service::FieldResult<response::Value> Log::getData(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Log::getData is not implemented)ex");
}

std::future<service::ResolverResult> Log::resolveData(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getData(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Transaction>> Log::getTransaction(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Log::getTransaction is not implemented)ex");
}

std::future<service::ResolverResult> Log::resolveTransaction(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransaction(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Transaction>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Log::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Log)gql"},
                                                                std::move(params));
}

Transaction::Transaction()
    : service::Object(
          {"Transaction"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(block)gql"sv, [this](service::ResolverParams&& params) { return resolveBlock(std::move(params)); }},
           {R"gql(createdContract)gql"sv,
            [this](service::ResolverParams&& params) { return resolveCreatedContract(std::move(params)); }},
           {R"gql(cumulativeGasUsed)gql"sv,
            [this](service::ResolverParams&& params) { return resolveCumulativeGasUsed(std::move(params)); }},
           {R"gql(from)gql"sv, [this](service::ResolverParams&& params) { return resolveFrom(std::move(params)); }},
           {R"gql(gas)gql"sv, [this](service::ResolverParams&& params) { return resolveGas(std::move(params)); }},
           {R"gql(gasPrice)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasPrice(std::move(params)); }},
           {R"gql(gasUsed)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasUsed(std::move(params)); }},
           {R"gql(hash)gql"sv, [this](service::ResolverParams&& params) { return resolveHash(std::move(params)); }},
           {R"gql(index)gql"sv, [this](service::ResolverParams&& params) { return resolveIndex(std::move(params)); }},
           {R"gql(inputData)gql"sv,
            [this](service::ResolverParams&& params) { return resolveInputData(std::move(params)); }},
           {R"gql(logs)gql"sv, [this](service::ResolverParams&& params) { return resolveLogs(std::move(params)); }},
           {R"gql(nonce)gql"sv, [this](service::ResolverParams&& params) { return resolveNonce(std::move(params)); }},
           {R"gql(r)gql"sv, [this](service::ResolverParams&& params) { return resolveR(std::move(params)); }},
           {R"gql(s)gql"sv, [this](service::ResolverParams&& params) { return resolveS(std::move(params)); }},
           {R"gql(status)gql"sv, [this](service::ResolverParams&& params) { return resolveStatus(std::move(params)); }},
           {R"gql(to)gql"sv, [this](service::ResolverParams&& params) { return resolveTo(std::move(params)); }},
           {R"gql(v)gql"sv, [this](service::ResolverParams&& params) { return resolveV(std::move(params)); }},
           {R"gql(value)gql"sv,
            [this](service::ResolverParams&& params) { return resolveValue(std::move(params)); }}}) {}

service::FieldResult<response::Value> Transaction::getHash(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getHash is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveHash(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getHash(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getNonce(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getNonce is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveNonce(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getNonce(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::IntType>> Transaction::getIndex(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getIndex is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveIndex(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getIndex(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                              std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Transaction::getFrom(service::FieldParams&&,
                                                                    std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Transaction::getFrom is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveFrom(service::ResolverParams&& params) {
  auto argBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("block", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getFrom(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argBlock));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Transaction::getTo(service::FieldParams&&,
                                                                  std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Transaction::getTo is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveTo(service::ResolverParams&& params) {
  auto argBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("block", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTo(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argBlock));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                    std::move(params));
}

service::FieldResult<response::Value> Transaction::getValue(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getValue is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveValue(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getValue(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getGasPrice(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getGasPrice is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveGasPrice(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasPrice(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getGas(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getGas is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveGas(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGas(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getInputData(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getInputData is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveInputData(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getInputData(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Block>> Transaction::getBlock(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getBlock is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveBlock(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getBlock(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::Value>> Transaction::getStatus(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getStatus is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveStatus(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getStatus(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                            std::move(params));
}

service::FieldResult<std::optional<response::Value>> Transaction::getGasUsed(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getGasUsed is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveGasUsed(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasUsed(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                            std::move(params));
}

service::FieldResult<std::optional<response::Value>> Transaction::getCumulativeGasUsed(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getCumulativeGasUsed is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveCumulativeGasUsed(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getCumulativeGasUsed(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                            std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Transaction::getCreatedContract(service::FieldParams&&,
                                                                               std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Transaction::getCreatedContract is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveCreatedContract(service::ResolverParams&& params) {
  auto argBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("block", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result =
      getCreatedContract(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argBlock));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                    std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<Log>>>> Transaction::getLogs(
    service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getLogs is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveLogs(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getLogs(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Log>::convert<service::TypeModifier::Nullable, service::TypeModifier::List>(
      std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getR(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getR is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveR(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getR(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getS(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getS is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveS(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getS(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Transaction::getV(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Transaction::getV is not implemented)ex");
}

std::future<service::ResolverResult> Transaction::resolveV(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getV(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Transaction::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Transaction)gql"},
                                                                std::move(params));
}

Block::Block()
    : service::Object(
          {"Block"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(account)gql"sv,
            [this](service::ResolverParams&& params) { return resolveAccount(std::move(params)); }},
           {R"gql(call)gql"sv, [this](service::ResolverParams&& params) { return resolveCall(std::move(params)); }},
           {R"gql(difficulty)gql"sv,
            [this](service::ResolverParams&& params) { return resolveDifficulty(std::move(params)); }},
           {R"gql(estimateGas)gql"sv,
            [this](service::ResolverParams&& params) { return resolveEstimateGas(std::move(params)); }},
           {R"gql(extraData)gql"sv,
            [this](service::ResolverParams&& params) { return resolveExtraData(std::move(params)); }},
           {R"gql(gasLimit)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasLimit(std::move(params)); }},
           {R"gql(gasUsed)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasUsed(std::move(params)); }},
           {R"gql(hash)gql"sv, [this](service::ResolverParams&& params) { return resolveHash(std::move(params)); }},
           {R"gql(logs)gql"sv, [this](service::ResolverParams&& params) { return resolveLogs(std::move(params)); }},
           {R"gql(logsBloom)gql"sv,
            [this](service::ResolverParams&& params) { return resolveLogsBloom(std::move(params)); }},
           {R"gql(miner)gql"sv, [this](service::ResolverParams&& params) { return resolveMiner(std::move(params)); }},
           {R"gql(mixHash)gql"sv,
            [this](service::ResolverParams&& params) { return resolveMixHash(std::move(params)); }},
           {R"gql(nonce)gql"sv, [this](service::ResolverParams&& params) { return resolveNonce(std::move(params)); }},
           {R"gql(number)gql"sv, [this](service::ResolverParams&& params) { return resolveNumber(std::move(params)); }},
           {R"gql(ommerAt)gql"sv,
            [this](service::ResolverParams&& params) { return resolveOmmerAt(std::move(params)); }},
           {R"gql(ommerCount)gql"sv,
            [this](service::ResolverParams&& params) { return resolveOmmerCount(std::move(params)); }},
           {R"gql(ommerHash)gql"sv,
            [this](service::ResolverParams&& params) { return resolveOmmerHash(std::move(params)); }},
           {R"gql(ommers)gql"sv, [this](service::ResolverParams&& params) { return resolveOmmers(std::move(params)); }},
           {R"gql(parent)gql"sv, [this](service::ResolverParams&& params) { return resolveParent(std::move(params)); }},
           {R"gql(receiptsRoot)gql"sv,
            [this](service::ResolverParams&& params) { return resolveReceiptsRoot(std::move(params)); }},
           {R"gql(stateRoot)gql"sv,
            [this](service::ResolverParams&& params) { return resolveStateRoot(std::move(params)); }},
           {R"gql(timestamp)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTimestamp(std::move(params)); }},
           {R"gql(totalDifficulty)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTotalDifficulty(std::move(params)); }},
           {R"gql(transactionAt)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactionAt(std::move(params)); }},
           {R"gql(transactionCount)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactionCount(std::move(params)); }},
           {R"gql(transactions)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactions(std::move(params)); }},
           {R"gql(transactionsRoot)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactionsRoot(std::move(params)); }}}) {}

service::FieldResult<response::Value> Block::getNumber(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getNumber is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveNumber(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getNumber(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getHash(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getHash is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveHash(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getHash(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Block>> Block::getParent(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getParent is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveParent(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getParent(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getNonce(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getNonce is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveNonce(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getNonce(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getTransactionsRoot(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getTransactionsRoot is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTransactionsRoot(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactionsRoot(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::IntType>> Block::getTransactionCount(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getTransactionCount is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTransactionCount(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactionCount(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                              std::move(params));
}

service::FieldResult<response::Value> Block::getStateRoot(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getStateRoot is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveStateRoot(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getStateRoot(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getReceiptsRoot(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getReceiptsRoot is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveReceiptsRoot(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getReceiptsRoot(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Block::getMiner(service::FieldParams&&,
                                                               std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Block::getMiner is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveMiner(service::ResolverParams&& params) {
  auto argBlock =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("block", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getMiner(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argBlock));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getExtraData(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getExtraData is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveExtraData(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getExtraData(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getGasLimit(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getGasLimit is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveGasLimit(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasLimit(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getGasUsed(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getGasUsed is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveGasUsed(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasUsed(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getTimestamp(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getTimestamp is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTimestamp(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTimestamp(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getLogsBloom(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getLogsBloom is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveLogsBloom(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getLogsBloom(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getMixHash(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getMixHash is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveMixHash(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getMixHash(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getDifficulty(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getDifficulty is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveDifficulty(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getDifficulty(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getTotalDifficulty(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getTotalDifficulty is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTotalDifficulty(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTotalDifficulty(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::IntType>> Block::getOmmerCount(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getOmmerCount is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveOmmerCount(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getOmmerCount(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                              std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<Block>>>> Block::getOmmers(
    service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getOmmers is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveOmmers(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getOmmers(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::Nullable, service::TypeModifier::List,
                                                 service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Block>> Block::getOmmerAt(service::FieldParams&&, response::IntType&&) const {
  throw std::runtime_error(R"ex(Block::getOmmerAt is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveOmmerAt(service::ResolverParams&& params) {
  auto argIndex = service::ModifiedArgument<response::IntType>::require("index", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getOmmerAt(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argIndex));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Block::getOmmerHash(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getOmmerHash is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveOmmerHash(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getOmmerHash(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<Transaction>>>> Block::getTransactions(
    service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Block::getTransactions is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTransactions(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactions(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable, service::TypeModifier::List>(
      std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Transaction>> Block::getTransactionAt(service::FieldParams&&,
                                                                           response::IntType&&) const {
  throw std::runtime_error(R"ex(Block::getTransactionAt is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveTransactionAt(service::ResolverParams&& params) {
  auto argIndex = service::ModifiedArgument<response::IntType>::require("index", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactionAt(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argIndex));
  resolverLock.unlock();

  return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                        std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Log>>> Block::getLogs(service::FieldParams&&,
                                                                       BlockFilterCriteria&&) const {
  throw std::runtime_error(R"ex(Block::getLogs is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveLogs(service::ResolverParams&& params) {
  auto argFilter = service::ModifiedArgument<taraxa::BlockFilterCriteria>::require("filter", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getLogs(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFilter));
  resolverLock.unlock();

  return service::ModifiedResult<Log>::convert<service::TypeModifier::List>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Block::getAccount(service::FieldParams&&, response::Value&&) const {
  throw std::runtime_error(R"ex(Block::getAccount is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveAccount(service::ResolverParams&& params) {
  auto argAddress = service::ModifiedArgument<response::Value>::require("address", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getAccount(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argAddress));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<CallResult>> Block::getCall(service::FieldParams&&, CallData&&) const {
  throw std::runtime_error(R"ex(Block::getCall is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveCall(service::ResolverParams&& params) {
  auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getCall(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argData));
  resolverLock.unlock();

  return service::ModifiedResult<CallResult>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                       std::move(params));
}

service::FieldResult<response::Value> Block::getEstimateGas(service::FieldParams&&, CallData&&) const {
  throw std::runtime_error(R"ex(Block::getEstimateGas is not implemented)ex");
}

std::future<service::ResolverResult> Block::resolveEstimateGas(service::ResolverParams&& params) {
  auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getEstimateGas(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argData));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Block::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Block)gql"},
                                                                std::move(params));
}

CallResult::CallResult()
    : service::Object(
          {"CallResult"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(data)gql"sv, [this](service::ResolverParams&& params) { return resolveData(std::move(params)); }},
           {R"gql(gasUsed)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasUsed(std::move(params)); }},
           {R"gql(status)gql"sv,
            [this](service::ResolverParams&& params) { return resolveStatus(std::move(params)); }}}) {}

service::FieldResult<response::Value> CallResult::getData(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(CallResult::getData is not implemented)ex");
}

std::future<service::ResolverResult> CallResult::resolveData(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getData(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> CallResult::getGasUsed(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(CallResult::getGasUsed is not implemented)ex");
}

std::future<service::ResolverResult> CallResult::resolveGasUsed(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasUsed(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> CallResult::getStatus(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(CallResult::getStatus is not implemented)ex");
}

std::future<service::ResolverResult> CallResult::resolveStatus(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getStatus(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> CallResult::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(CallResult)gql"},
                                                                std::move(params));
}

SyncState::SyncState()
    : service::Object({"SyncState"},
                      {{R"gql(__typename)gql"sv,
                        [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
                       {R"gql(currentBlock)gql"sv,
                        [this](service::ResolverParams&& params) { return resolveCurrentBlock(std::move(params)); }},
                       {R"gql(highestBlock)gql"sv,
                        [this](service::ResolverParams&& params) { return resolveHighestBlock(std::move(params)); }},
                       {R"gql(knownStates)gql"sv,
                        [this](service::ResolverParams&& params) { return resolveKnownStates(std::move(params)); }},
                       {R"gql(pulledStates)gql"sv,
                        [this](service::ResolverParams&& params) { return resolvePulledStates(std::move(params)); }},
                       {R"gql(startingBlock)gql"sv, [this](service::ResolverParams&& params) {
                          return resolveStartingBlock(std::move(params));
                        }}}) {}

service::FieldResult<response::Value> SyncState::getStartingBlock(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(SyncState::getStartingBlock is not implemented)ex");
}

std::future<service::ResolverResult> SyncState::resolveStartingBlock(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getStartingBlock(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> SyncState::getCurrentBlock(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(SyncState::getCurrentBlock is not implemented)ex");
}

std::future<service::ResolverResult> SyncState::resolveCurrentBlock(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getCurrentBlock(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::Value> SyncState::getHighestBlock(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(SyncState::getHighestBlock is not implemented)ex");
}

std::future<service::ResolverResult> SyncState::resolveHighestBlock(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getHighestBlock(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::Value>> SyncState::getPulledStates(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(SyncState::getPulledStates is not implemented)ex");
}

std::future<service::ResolverResult> SyncState::resolvePulledStates(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getPulledStates(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                            std::move(params));
}

service::FieldResult<std::optional<response::Value>> SyncState::getKnownStates(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(SyncState::getKnownStates is not implemented)ex");
}

std::future<service::ResolverResult> SyncState::resolveKnownStates(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getKnownStates(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                            std::move(params));
}

std::future<service::ResolverResult> SyncState::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(SyncState)gql"},
                                                                std::move(params));
}

Pending::Pending()
    : service::Object(
          {"Pending"},
          {{R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(account)gql"sv,
            [this](service::ResolverParams&& params) { return resolveAccount(std::move(params)); }},
           {R"gql(call)gql"sv, [this](service::ResolverParams&& params) { return resolveCall(std::move(params)); }},
           {R"gql(estimateGas)gql"sv,
            [this](service::ResolverParams&& params) { return resolveEstimateGas(std::move(params)); }},
           {R"gql(transactionCount)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactionCount(std::move(params)); }},
           {R"gql(transactions)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransactions(std::move(params)); }}}) {}

service::FieldResult<response::IntType> Pending::getTransactionCount(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Pending::getTransactionCount is not implemented)ex");
}

std::future<service::ResolverResult> Pending::resolveTransactionCount(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactionCount(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<Transaction>>>> Pending::getTransactions(
    service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Pending::getTransactions is not implemented)ex");
}

std::future<service::ResolverResult> Pending::resolveTransactions(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransactions(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable, service::TypeModifier::List>(
      std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Account>> Pending::getAccount(service::FieldParams&&, response::Value&&) const {
  throw std::runtime_error(R"ex(Pending::getAccount is not implemented)ex");
}

std::future<service::ResolverResult> Pending::resolveAccount(service::ResolverParams&& params) {
  auto argAddress = service::ModifiedArgument<response::Value>::require("address", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getAccount(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argAddress));
  resolverLock.unlock();

  return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<CallResult>> Pending::getCall(service::FieldParams&&, CallData&&) const {
  throw std::runtime_error(R"ex(Pending::getCall is not implemented)ex");
}

std::future<service::ResolverResult> Pending::resolveCall(service::ResolverParams&& params) {
  auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getCall(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argData));
  resolverLock.unlock();

  return service::ModifiedResult<CallResult>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                       std::move(params));
}

service::FieldResult<response::Value> Pending::getEstimateGas(service::FieldParams&&, CallData&&) const {
  throw std::runtime_error(R"ex(Pending::getEstimateGas is not implemented)ex");
}

std::future<service::ResolverResult> Pending::resolveEstimateGas(service::ResolverParams&& params) {
  auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getEstimateGas(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argData));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Pending::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Pending)gql"},
                                                                std::move(params));
}

Query::Query()
    : service::Object(
          {"Query"},
          {{R"gql(__schema)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_schema(std::move(params)); }},
           {R"gql(__type)gql"sv, [this](service::ResolverParams&& params) { return resolve_type(std::move(params)); }},
           {R"gql(__typename)gql"sv,
            [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
           {R"gql(block)gql"sv, [this](service::ResolverParams&& params) { return resolveBlock(std::move(params)); }},
           {R"gql(blocks)gql"sv, [this](service::ResolverParams&& params) { return resolveBlocks(std::move(params)); }},
           {R"gql(chainID)gql"sv,
            [this](service::ResolverParams&& params) { return resolveChainID(std::move(params)); }},
           {R"gql(gasPrice)gql"sv,
            [this](service::ResolverParams&& params) { return resolveGasPrice(std::move(params)); }},
           {R"gql(logs)gql"sv, [this](service::ResolverParams&& params) { return resolveLogs(std::move(params)); }},
           {R"gql(pending)gql"sv,
            [this](service::ResolverParams&& params) { return resolvePending(std::move(params)); }},
           {R"gql(protocolVersion)gql"sv,
            [this](service::ResolverParams&& params) { return resolveProtocolVersion(std::move(params)); }},
           {R"gql(syncing)gql"sv,
            [this](service::ResolverParams&& params) { return resolveSyncing(std::move(params)); }},
           {R"gql(transaction)gql"sv,
            [this](service::ResolverParams&& params) { return resolveTransaction(std::move(params)); }}}),
      _schema(GetSchema()) {}

service::FieldResult<std::shared_ptr<Block>> Query::getBlock(service::FieldParams&&, std::optional<response::Value>&&,
                                                             std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Query::getBlock is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveBlock(service::ResolverParams&& params) {
  auto argNumber =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("number", params.arguments);
  auto argHash =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("hash", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getBlock(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argNumber),
                         std::move(argHash));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Block>>> Query::getBlocks(service::FieldParams&&, response::Value&&,
                                                                           std::optional<response::Value>&&) const {
  throw std::runtime_error(R"ex(Query::getBlocks is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveBlocks(service::ResolverParams&& params) {
  auto argFrom = service::ModifiedArgument<response::Value>::require("from", params.arguments);
  auto argTo =
      service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("to", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result =
      getBlocks(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFrom), std::move(argTo));
  resolverLock.unlock();

  return service::ModifiedResult<Block>::convert<service::TypeModifier::List>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Pending>> Query::getPending(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Query::getPending is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolvePending(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getPending(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<Pending>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<Transaction>> Query::getTransaction(service::FieldParams&&,
                                                                         response::Value&&) const {
  throw std::runtime_error(R"ex(Query::getTransaction is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveTransaction(service::ResolverParams&& params) {
  auto argHash = service::ModifiedArgument<response::Value>::require("hash", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getTransaction(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argHash));
  resolverLock.unlock();

  return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                        std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Log>>> Query::getLogs(service::FieldParams&&, FilterCriteria&&) const {
  throw std::runtime_error(R"ex(Query::getLogs is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveLogs(service::ResolverParams&& params) {
  auto argFilter = service::ModifiedArgument<taraxa::FilterCriteria>::require("filter", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getLogs(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFilter));
  resolverLock.unlock();

  return service::ModifiedResult<Log>::convert<service::TypeModifier::List>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> Query::getGasPrice(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Query::getGasPrice is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveGasPrice(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getGasPrice(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::IntType> Query::getProtocolVersion(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Query::getProtocolVersion is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveProtocolVersion(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getProtocolVersion(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<SyncState>> Query::getSyncing(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Query::getSyncing is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveSyncing(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getSyncing(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<SyncState>::convert<service::TypeModifier::Nullable>(std::move(result),
                                                                                      std::move(params));
}

service::FieldResult<response::Value> Query::getChainID(service::FieldParams&&) const {
  throw std::runtime_error(R"ex(Query::getChainID is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveChainID(service::ResolverParams&& params) {
  std::unique_lock resolverLock(_resolverMutex);
  auto result = getChainID(service::FieldParams(params, std::move(params.fieldDirectives)));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Query::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Query)gql"},
                                                                std::move(params));
}

std::future<service::ResolverResult> Query::resolve_schema(service::ResolverParams&& params) {
  return service::ModifiedResult<service::Object>::convert(
      std::static_pointer_cast<service::Object>(std::make_shared<introspection::Schema>(_schema)), std::move(params));
}

std::future<service::ResolverResult> Query::resolve_type(service::ResolverParams&& params) {
  auto argName = service::ModifiedArgument<response::StringType>::require("name", params.arguments);
  const auto& baseType = _schema->LookupType(argName);
  std::shared_ptr<introspection::object::Type> result{baseType ? std::make_shared<introspection::Type>(baseType)
                                                               : nullptr};

  return service::ModifiedResult<introspection::object::Type>::convert<service::TypeModifier::Nullable>(
      result, std::move(params));
}

Mutation::Mutation()
    : service::Object({"Mutation"},
                      {{R"gql(__typename)gql"sv,
                        [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); }},
                       {R"gql(sendRawTransaction)gql"sv, [this](service::ResolverParams&& params) {
                          return resolveSendRawTransaction(std::move(params));
                        }}}) {}

service::FieldResult<response::Value> Mutation::applySendRawTransaction(service::FieldParams&&,
                                                                        response::Value&&) const {
  throw std::runtime_error(R"ex(Mutation::applySendRawTransaction is not implemented)ex");
}

std::future<service::ResolverResult> Mutation::resolveSendRawTransaction(service::ResolverParams&& params) {
  auto argData = service::ModifiedArgument<response::Value>::require("data", params.arguments);
  std::unique_lock resolverLock(_resolverMutex);
  auto result =
      applySendRawTransaction(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argData));
  resolverLock.unlock();

  return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Mutation::resolve_typename(service::ResolverParams&& params) {
  return service::ModifiedResult<response::StringType>::convert(response::StringType{R"gql(Mutation)gql"},
                                                                std::move(params));
}

} /* namespace object */

Operations::Operations(std::shared_ptr<object::Query> query, std::shared_ptr<object::Mutation> mutation)
    : service::Request({{"query", query}, {"mutation", mutation}}, GetSchema()),
      _query(std::move(query)),
      _mutation(std::move(mutation)) {}

void AddTypesToSchema(const std::shared_ptr<schema::Schema>& schema) {
  schema->AddType(R"gql(Bytes32)gql"sv, schema::ScalarType::Make(R"gql(Bytes32)gql"sv, R"md()md"));
  schema->AddType(R"gql(Address)gql"sv, schema::ScalarType::Make(R"gql(Address)gql"sv, R"md()md"));
  schema->AddType(R"gql(Bytes)gql"sv, schema::ScalarType::Make(R"gql(Bytes)gql"sv, R"md()md"));
  schema->AddType(R"gql(BigInt)gql"sv, schema::ScalarType::Make(R"gql(BigInt)gql"sv, R"md()md"));
  schema->AddType(R"gql(Long)gql"sv, schema::ScalarType::Make(R"gql(Long)gql"sv, R"md()md"));
  auto typeBlockFilterCriteria = schema::InputObjectType::Make(R"gql(BlockFilterCriteria)gql"sv, R"md()md"sv);
  schema->AddType(R"gql(BlockFilterCriteria)gql"sv, typeBlockFilterCriteria);
  auto typeCallData = schema::InputObjectType::Make(R"gql(CallData)gql"sv, R"md()md"sv);
  schema->AddType(R"gql(CallData)gql"sv, typeCallData);
  auto typeFilterCriteria = schema::InputObjectType::Make(R"gql(FilterCriteria)gql"sv, R"md()md"sv);
  schema->AddType(R"gql(FilterCriteria)gql"sv, typeFilterCriteria);
  auto typeAccount = schema::ObjectType::Make(R"gql(Account)gql"sv, R"md()md");
  schema->AddType(R"gql(Account)gql"sv, typeAccount);
  auto typeLog = schema::ObjectType::Make(R"gql(Log)gql"sv, R"md()md");
  schema->AddType(R"gql(Log)gql"sv, typeLog);
  auto typeTransaction = schema::ObjectType::Make(R"gql(Transaction)gql"sv, R"md()md");
  schema->AddType(R"gql(Transaction)gql"sv, typeTransaction);
  auto typeBlock = schema::ObjectType::Make(R"gql(Block)gql"sv, R"md()md");
  schema->AddType(R"gql(Block)gql"sv, typeBlock);
  auto typeCallResult = schema::ObjectType::Make(R"gql(CallResult)gql"sv, R"md()md");
  schema->AddType(R"gql(CallResult)gql"sv, typeCallResult);
  auto typeSyncState = schema::ObjectType::Make(R"gql(SyncState)gql"sv, R"md()md");
  schema->AddType(R"gql(SyncState)gql"sv, typeSyncState);
  auto typePending = schema::ObjectType::Make(R"gql(Pending)gql"sv, R"md()md");
  schema->AddType(R"gql(Pending)gql"sv, typePending);
  auto typeQuery = schema::ObjectType::Make(R"gql(Query)gql"sv, R"md()md");
  schema->AddType(R"gql(Query)gql"sv, typeQuery);
  auto typeMutation = schema::ObjectType::Make(R"gql(Mutation)gql"sv, R"md()md");
  schema->AddType(R"gql(Mutation)gql"sv, typeMutation);

  typeBlockFilterCriteria->AddInputValues(
      {schema::InputValue::Make(
           R"gql(addresses)gql"sv, R"md()md"sv,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Address"))),
           R"gql()gql"sv),
       schema::InputValue::Make(
           R"gql(topics)gql"sv, R"md()md"sv,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL,
                                             schema->WrapType(introspection::TypeKind::LIST,
                                                              schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                               schema->LookupType("Bytes32"))))),
           R"gql()gql"sv)});
  typeCallData->AddInputValues(
      {schema::InputValue::Make(R"gql(from)gql"sv, R"md()md"sv, schema->LookupType("Address"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(to)gql"sv, R"md()md"sv, schema->LookupType("Address"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(gas)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(gasPrice)gql"sv, R"md()md"sv, schema->LookupType("BigInt"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(value)gql"sv, R"md()md"sv, schema->LookupType("BigInt"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(data)gql"sv, R"md()md"sv, schema->LookupType("Bytes"), R"gql()gql"sv)});
  typeFilterCriteria->AddInputValues(
      {schema::InputValue::Make(R"gql(fromBlock)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv),
       schema::InputValue::Make(R"gql(toBlock)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv),
       schema::InputValue::Make(
           R"gql(addresses)gql"sv, R"md()md"sv,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Address"))),
           R"gql()gql"sv),
       schema::InputValue::Make(
           R"gql(topics)gql"sv, R"md()md"sv,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL,
                                             schema->WrapType(introspection::TypeKind::LIST,
                                                              schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                               schema->LookupType("Bytes32"))))),
           R"gql()gql"sv)});

  typeAccount->AddFields(
      {schema::Field::Make(R"gql(address)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Address"))),
       schema::Field::Make(R"gql(balance)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(transactionCount)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(code)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(
           R"gql(storage)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32")),
           {schema::InputValue::Make(R"gql(slot)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32")),
                                     R"gql()gql"sv)})});
  typeLog->AddFields(
      {schema::Field::Make(R"gql(index)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int"))),
       schema::Field::Make(
           R"gql(account)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Account")),
           {schema::InputValue::Make(R"gql(block)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(R"gql(topics)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL,
                                            schema->WrapType(introspection::TypeKind::LIST,
                                                             schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                              schema->LookupType("Bytes32"))))),
       schema::Field::Make(R"gql(data)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(transaction)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Transaction")))});
  typeTransaction->AddFields(
      {schema::Field::Make(R"gql(hash)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(R"gql(nonce)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(index)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Int")),
       schema::Field::Make(
           R"gql(from)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Account")),
           {schema::InputValue::Make(R"gql(block)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(to)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Account"),
           {schema::InputValue::Make(R"gql(block)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(R"gql(value)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(gasPrice)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(gas)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(inputData)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(block)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Block")),
       schema::Field::Make(R"gql(status)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Long")),
       schema::Field::Make(R"gql(gasUsed)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Long")),
       schema::Field::Make(R"gql(cumulativeGasUsed)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Long")),
       schema::Field::Make(
           R"gql(createdContract)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Account"),
           {schema::InputValue::Make(R"gql(block)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(logs)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Log")))),
       schema::Field::Make(R"gql(r)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(s)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(v)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt")))});
  typeBlock->AddFields(
      {schema::Field::Make(R"gql(number)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(hash)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(R"gql(parent)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Block")),
       schema::Field::Make(R"gql(nonce)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(transactionsRoot)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(R"gql(transactionCount)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Int")),
       schema::Field::Make(R"gql(stateRoot)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(R"gql(receiptsRoot)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(
           R"gql(miner)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Account")),
           {schema::InputValue::Make(R"gql(block)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(R"gql(extraData)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(gasLimit)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(gasUsed)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(timestamp)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(logsBloom)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(mixHash)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(R"gql(difficulty)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(totalDifficulty)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(ommerCount)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Int")),
       schema::Field::Make(R"gql(ommers)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("Block"))),
       schema::Field::Make(
           R"gql(ommerAt)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Block"),
           {schema::InputValue::Make(R"gql(index)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int")),
                                     R"gql()gql"sv)}),
       schema::Field::Make(R"gql(ommerHash)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32"))),
       schema::Field::Make(
           R"gql(transactions)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Transaction")))),
       schema::Field::Make(
           R"gql(transactionAt)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Transaction"),
           {schema::InputValue::Make(R"gql(index)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int")),
                                     R"gql()gql"sv)}),
       schema::Field::Make(R"gql(logs)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL,
                                            schema->WrapType(introspection::TypeKind::LIST,
                                                             schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                              schema->LookupType("Log")))),
                           {schema::InputValue::Make(R"gql(filter)gql"sv, R"md()md"sv,
                                                     schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                      schema->LookupType("BlockFilterCriteria")),
                                                     R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(account)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Account")),
           {schema::InputValue::Make(R"gql(address)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Address")),
                                     R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(call)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("CallResult"),
           {schema::InputValue::Make(
               R"gql(data)gql"sv, R"md()md"sv,
               schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CallData")), R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(estimateGas)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long")),
           {schema::InputValue::Make(
               R"gql(data)gql"sv, R"md()md"sv,
               schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CallData")), R"gql()gql"sv)})});
  typeCallResult->AddFields(
      {schema::Field::Make(R"gql(data)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes"))),
       schema::Field::Make(R"gql(gasUsed)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(status)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long")))});
  typeSyncState->AddFields(
      {schema::Field::Make(R"gql(startingBlock)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(currentBlock)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(highestBlock)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long"))),
       schema::Field::Make(R"gql(pulledStates)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Long")),
       schema::Field::Make(R"gql(knownStates)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Long"))});
  typePending->AddFields(
      {schema::Field::Make(R"gql(transactionCount)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int"))),
       schema::Field::Make(
           R"gql(transactions)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::LIST,
                            schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Transaction")))),
       schema::Field::Make(
           R"gql(account)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Account")),
           {schema::InputValue::Make(R"gql(address)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Address")),
                                     R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(call)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("CallResult"),
           {schema::InputValue::Make(
               R"gql(data)gql"sv, R"md()md"sv,
               schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CallData")), R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(estimateGas)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long")),
           {schema::InputValue::Make(
               R"gql(data)gql"sv, R"md()md"sv,
               schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CallData")), R"gql()gql"sv)})});
  typeQuery->AddFields(
      {schema::Field::Make(
           R"gql(block)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Block"),
           {schema::InputValue::Make(R"gql(number)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv),
            schema::InputValue::Make(R"gql(hash)gql"sv, R"md()md"sv, schema->LookupType("Bytes32"), R"gql()gql"sv)}),
       schema::Field::Make(
           R"gql(blocks)gql"sv, R"md()md"sv, std::nullopt,
           schema->WrapType(
               introspection::TypeKind::NON_NULL,
               schema->WrapType(introspection::TypeKind::LIST,
                                schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Block")))),
           {schema::InputValue::Make(R"gql(from)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Long")),
                                     R"gql()gql"sv),
            schema::InputValue::Make(R"gql(to)gql"sv, R"md()md"sv, schema->LookupType("Long"), R"gql()gql"sv)}),
       schema::Field::Make(R"gql(pending)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Pending"))),
       schema::Field::Make(
           R"gql(transaction)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Transaction"),
           {schema::InputValue::Make(R"gql(hash)gql"sv, R"md()md"sv,
                                     schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32")),
                                     R"gql()gql"sv)}),
       schema::Field::Make(R"gql(logs)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL,
                                            schema->WrapType(introspection::TypeKind::LIST,
                                                             schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                              schema->LookupType("Log")))),
                           {schema::InputValue::Make(R"gql(filter)gql"sv, R"md()md"sv,
                                                     schema->WrapType(introspection::TypeKind::NON_NULL,
                                                                      schema->LookupType("FilterCriteria")),
                                                     R"gql()gql"sv)}),
       schema::Field::Make(R"gql(gasPrice)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt"))),
       schema::Field::Make(R"gql(protocolVersion)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int"))),
       schema::Field::Make(R"gql(syncing)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("SyncState")),
       schema::Field::Make(R"gql(chainID)gql"sv, R"md()md"sv, std::nullopt,
                           schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("BigInt")))});
  typeMutation->AddFields({schema::Field::Make(
      R"gql(sendRawTransaction)gql"sv, R"md()md"sv, std::nullopt,
      schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes32")),
      {schema::InputValue::Make(R"gql(data)gql"sv, R"md()md"sv,
                                schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Bytes")),
                                R"gql()gql"sv)})});

  schema->AddQueryType(typeQuery);
  schema->AddMutationType(typeMutation);
}

std::shared_ptr<schema::Schema> GetSchema() {
  static std::weak_ptr<schema::Schema> s_wpSchema;
  auto schema = s_wpSchema.lock();

  if (!schema) {
    schema = std::make_shared<schema::Schema>(false);
    introspection::AddTypesToSchema(schema);
    AddTypesToSchema(schema);
    s_wpSchema = schema;
  }

  return schema;
}

} /* namespace taraxa */
} /* namespace graphql */
