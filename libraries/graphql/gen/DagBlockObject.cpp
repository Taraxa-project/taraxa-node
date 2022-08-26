// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// WARNING! Do not edit this file manually, your changes will be overwritten.

#include "DagBlockObject.h"
#include "AccountObject.h"
#include "TransactionObject.h"

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

DagBlock::DagBlock(std::unique_ptr<const Concept>&& pimpl) noexcept
	: service::Object{ getTypeNames(), getResolvers() }
	, _pimpl { std::move(pimpl) }
{
}

service::TypeNames DagBlock::getTypeNames() const noexcept
{
	return {
		R"gql(DagBlock)gql"sv
	};
}

service::ResolverMap DagBlock::getResolvers() const noexcept
{
	return {
		{ R"gql(hash)gql"sv, [this](service::ResolverParams&& params) { return resolveHash(std::move(params)); } },
		{ R"gql(tips)gql"sv, [this](service::ResolverParams&& params) { return resolveTips(std::move(params)); } },
		{ R"gql(level)gql"sv, [this](service::ResolverParams&& params) { return resolveLevel(std::move(params)); } },
		{ R"gql(pivot)gql"sv, [this](service::ResolverParams&& params) { return resolvePivot(std::move(params)); } },
		{ R"gql(author)gql"sv, [this](service::ResolverParams&& params) { return resolveAuthor(std::move(params)); } },
		{ R"gql(timestamp)gql"sv, [this](service::ResolverParams&& params) { return resolveTimestamp(std::move(params)); } },
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(pbftPeriod)gql"sv, [this](service::ResolverParams&& params) { return resolvePbftPeriod(std::move(params)); } },
		{ R"gql(transactions)gql"sv, [this](service::ResolverParams&& params) { return resolveTransactions(std::move(params)); } }
	};
}

void DagBlock::beginSelectionSet(const service::SelectionSetParams& params) const
{
	_pimpl->beginSelectionSet(params);
}

void DagBlock::endSelectionSet(const service::SelectionSetParams& params) const
{
	_pimpl->endSelectionSet(params);
}

service::AwaitableResolver DagBlock::resolveHash(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getHash(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolvePivot(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getPivot(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolveTips(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getTips(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert<service::TypeModifier::List>(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolveLevel(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getLevel(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolvePbftPeriod(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getPbftPeriod(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolveAuthor(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getAuthor(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<Account>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolveTimestamp(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getTimestamp(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolveTransactions(service::ResolverParams&& params) const
{
	std::unique_lock resolverLock(_resolverMutex);
	auto directives = std::move(params.fieldDirectives);
	auto result = _pimpl->getTransactions(service::FieldParams(service::SelectionSetParams{ params }, std::move(directives)));
	resolverLock.unlock();

	return service::ModifiedResult<Transaction>::convert<service::TypeModifier::Nullable, service::TypeModifier::List>(std::move(result), std::move(params));
}

service::AwaitableResolver DagBlock::resolve_typename(service::ResolverParams&& params) const
{
	return service::Result<std::string>::convert(std::string{ R"gql(DagBlock)gql" }, std::move(params));
}

} // namespace object

void AddDagBlockDetails(const std::shared_ptr<schema::ObjectType>& typeDagBlock, const std::shared_ptr<schema::Schema>& schema)
{
	typeDagBlock->AddFields({
		schema::Field::Make(R"gql(hash)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Bytes32)gql"sv))),
		schema::Field::Make(R"gql(pivot)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Bytes32)gql"sv))),
		schema::Field::Make(R"gql(tips)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Bytes32)gql"sv))))),
		schema::Field::Make(R"gql(level)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Long)gql"sv))),
		schema::Field::Make(R"gql(pbftPeriod)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType(R"gql(Long)gql"sv)),
		schema::Field::Make(R"gql(author)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Account)gql"sv))),
		schema::Field::Make(R"gql(timestamp)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Long)gql"sv))),
		schema::Field::Make(R"gql(transactions)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType(R"gql(Transaction)gql"sv))))
	});
}

} // namespace graphql::taraxa
