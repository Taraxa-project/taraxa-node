// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// WARNING! Do not edit this file manually, your changes will be overwritten.

#include "PendingObject.h"
#include "TransactionObject.h"
#include "AccountObject.h"
#include "CallResultObject.h"

#include "graphqlservice/internal/Schema.h"

#include "graphqlservice/introspection/IntrospectionSchema.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

using namespace std::literals;

namespace graphql::taraxa {
namespace object {

Pending::Pending(std::unique_ptr<const Concept>&& pimpl) noexcept
	: service::Object{ getTypeNames(), getResolvers() }
	, _pimpl { std::move(pimpl) }
{
}

service::TypeNames Pending::getTypeNames() const noexcept
{
	return {
		R"gql(Pending)gql"sv
	};
}

service::ResolverMap Pending::getResolvers() const noexcept
{
	return {
		{ R"gql(call)gql"sv, [this](service::ResolverParams&& params) { return resolveCall(std::move(params)); } },
		{ R"gql(account)gql"sv, [this](service::ResolverParams&& params) { return resolveAccount(std::move(params)); } },
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(estimateGas)gql"sv, [this](service::ResolverParams&& params) { return resolveEstimateGas(std::move(params)); } },
		{ R"gql(transactions)gql"sv, [this](service::ResolverParams&& params) { return resolveTransactions(std::move(params)); } },
		{ R"gql(transactionCount)gql"sv, [this](service::ResolverParams&& params) { return resolveTransactionCount(std::move(params)); } }
	};
}

void Pending::beginSelectionSet(const service::SelectionSetParams& params) const
{
	_pimpl->beginSelectionSet(params);
}

void Pending::endSelectionSet(const service::SelectionSetParams& params) const
{
	_pimpl->endSelectionSet(params);
}

service::AwaitableResolver Pending::resolveTransactionCount(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getTransactionCount(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<int>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver Pending::resolveTransactions(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getTransactions(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable, service::TypeModifier::List>(std::move(result), std::move(params));
}

service::AwaitableResolver Pending::resolveAccount(service::ResolverParams&& params) const
{
	auto argAddress = service::ModifiedArgument<response::Value>::require("address", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getAccount(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)), std::move(argAddress));
	resolverLock.unlock();

	return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver Pending::resolveCall(service::ResolverParams&& params) const
{
	auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getCall(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)), std::move(argData));
	resolverLock.unlock();

	return service::ModifiedResult<CallResult>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::AwaitableResolver Pending::resolveEstimateGas(service::ResolverParams&& params) const
{
	auto argData = service::ModifiedArgument<taraxa::CallData>::require("data", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getEstimateGas(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)), std::move(argData));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver Pending::resolve_typename(service::ResolverParams&& params) const
{
	return service::Result<std::string>::convert(std::string{ R"gql(Pending)gql" }, std::move(params));
}

} // namespace object

void AddPendingDetails(const std::shared_ptr<schema::ObjectType>& typePending, const std::shared_ptr<schema::Schema>& schema)
{
	typePending->AddFields({
		schema::Field::Make(R"gql(transactionCount)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Int)gql"sv))),
		schema::Field::Make(R"gql(transactions)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Transaction)gql"sv)))),
		schema::Field::Make(R"gql(account)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Account)gql"sv)), {
			schema::InputValue::Make(R"gql(address)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Address)gql"sv)), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(call)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType(R"gql(CallResult)gql"sv), {
			schema::InputValue::Make(R"gql(data)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(CallData)gql"sv)), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(estimateGas)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Long)gql"sv)), {
			schema::InputValue::Make(R"gql(data)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(CallData)gql"sv)), R"gql()gql"sv)
		})
	});
}

} // namespace graphql::taraxa
