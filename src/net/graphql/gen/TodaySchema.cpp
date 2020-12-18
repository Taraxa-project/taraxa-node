// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "TodaySchema.h"

#include "graphqlservice/introspection/Introspection.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

using namespace std::literals;

namespace graphql {
namespace service {

static const std::array<std::string_view, 4> s_namesTaskState = {
	"New",
	"Started",
	"Complete",
	"Unassigned"
};

template <>
today::TaskState ModifiedArgument<today::TaskState>::convert(const response::Value& value)
{
	if (!value.maybe_enum())
	{
		throw service::schema_exception { { "not a valid TaskState value" } };
	}

	auto itr = std::find(s_namesTaskState.cbegin(), s_namesTaskState.cend(), value.get<response::StringType>());

	if (itr == s_namesTaskState.cend())
	{
		throw service::schema_exception { { "not a valid TaskState value" } };
	}

	return static_cast<today::TaskState>(itr - s_namesTaskState.cbegin());
}

template <>
std::future<service::ResolverResult> ModifiedResult<today::TaskState>::convert(service::FieldResult<today::TaskState>&& result, ResolverParams&& params)
{
	return resolve(std::move(result), std::move(params),
		[](today::TaskState&& value, const ResolverParams&)
		{
			response::Value result(response::Type::EnumValue);

			result.set<response::StringType>(std::string(s_namesTaskState[static_cast<size_t>(value)]));

			return result;
		});
}

template <>
today::CompleteTaskInput ModifiedArgument<today::CompleteTaskInput>::convert(const response::Value& value)
{
	const auto defaultValue = []()
	{
		response::Value values(response::Type::Map);
		response::Value entry;

		entry = response::Value(true);
		values.emplace_back("isComplete", std::move(entry));

		return values;
	}();

	auto valueId = service::ModifiedArgument<response::IdType>::require("id", value);
	auto pairIsComplete = service::ModifiedArgument<response::BooleanType>::find<service::TypeModifier::Nullable>("isComplete", value);
	auto valueIsComplete = (pairIsComplete.second
		? std::move(pairIsComplete.first)
		: service::ModifiedArgument<response::BooleanType>::require<service::TypeModifier::Nullable>("isComplete", defaultValue));
	auto valueClientMutationId = service::ModifiedArgument<response::StringType>::require<service::TypeModifier::Nullable>("clientMutationId", value);

	return {
		std::move(valueId),
		std::move(valueIsComplete),
		std::move(valueClientMutationId)
	};
}

} /* namespace service */

namespace today {
namespace object {

Query::Query()
	: service::Object({
		"Query"
	}, {
		{ R"gql(__schema)gql"sv, [this](service::ResolverParams&& params) { return resolve_schema(std::move(params)); } },
		{ R"gql(__type)gql"sv, [this](service::ResolverParams&& params) { return resolve_type(std::move(params)); } },
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(appointments)gql"sv, [this](service::ResolverParams&& params) { return resolveAppointments(std::move(params)); } },
		{ R"gql(appointmentsById)gql"sv, [this](service::ResolverParams&& params) { return resolveAppointmentsById(std::move(params)); } },
		{ R"gql(expensive)gql"sv, [this](service::ResolverParams&& params) { return resolveExpensive(std::move(params)); } },
		{ R"gql(nested)gql"sv, [this](service::ResolverParams&& params) { return resolveNested(std::move(params)); } },
		{ R"gql(node)gql"sv, [this](service::ResolverParams&& params) { return resolveNode(std::move(params)); } },
		{ R"gql(tasks)gql"sv, [this](service::ResolverParams&& params) { return resolveTasks(std::move(params)); } },
		{ R"gql(tasksById)gql"sv, [this](service::ResolverParams&& params) { return resolveTasksById(std::move(params)); } },
		{ R"gql(unimplemented)gql"sv, [this](service::ResolverParams&& params) { return resolveUnimplemented(std::move(params)); } },
		{ R"gql(unreadCounts)gql"sv, [this](service::ResolverParams&& params) { return resolveUnreadCounts(std::move(params)); } },
		{ R"gql(unreadCountsById)gql"sv, [this](service::ResolverParams&& params) { return resolveUnreadCountsById(std::move(params)); } }
	})
	, _schema(GetSchema())
{
}

service::FieldResult<std::shared_ptr<service::Object>> Query::getNode(service::FieldParams&&, response::IdType&&) const
{
	throw std::runtime_error(R"ex(Query::getNode is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveNode(service::ResolverParams&& params)
{
	auto argId = service::ModifiedArgument<response::IdType>::require("id", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNode(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argId));
	resolverLock.unlock();

	return service::ModifiedResult<service::Object>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<AppointmentConnection>> Query::getAppointments(service::FieldParams&&, std::optional<response::IntType>&&, std::optional<response::Value>&&, std::optional<response::IntType>&&, std::optional<response::Value>&&) const
{
	throw std::runtime_error(R"ex(Query::getAppointments is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveAppointments(service::ResolverParams&& params)
{
	auto argFirst = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("first", params.arguments);
	auto argAfter = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("after", params.arguments);
	auto argLast = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("last", params.arguments);
	auto argBefore = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("before", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getAppointments(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFirst), std::move(argAfter), std::move(argLast), std::move(argBefore));
	resolverLock.unlock();

	return service::ModifiedResult<AppointmentConnection>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<TaskConnection>> Query::getTasks(service::FieldParams&&, std::optional<response::IntType>&&, std::optional<response::Value>&&, std::optional<response::IntType>&&, std::optional<response::Value>&&) const
{
	throw std::runtime_error(R"ex(Query::getTasks is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveTasks(service::ResolverParams&& params)
{
	auto argFirst = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("first", params.arguments);
	auto argAfter = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("after", params.arguments);
	auto argLast = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("last", params.arguments);
	auto argBefore = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("before", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getTasks(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFirst), std::move(argAfter), std::move(argLast), std::move(argBefore));
	resolverLock.unlock();

	return service::ModifiedResult<TaskConnection>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<FolderConnection>> Query::getUnreadCounts(service::FieldParams&&, std::optional<response::IntType>&&, std::optional<response::Value>&&, std::optional<response::IntType>&&, std::optional<response::Value>&&) const
{
	throw std::runtime_error(R"ex(Query::getUnreadCounts is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveUnreadCounts(service::ResolverParams&& params)
{
	auto argFirst = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("first", params.arguments);
	auto argAfter = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("after", params.arguments);
	auto argLast = service::ModifiedArgument<response::IntType>::require<service::TypeModifier::Nullable>("last", params.arguments);
	auto argBefore = service::ModifiedArgument<response::Value>::require<service::TypeModifier::Nullable>("before", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getUnreadCounts(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argFirst), std::move(argAfter), std::move(argLast), std::move(argBefore));
	resolverLock.unlock();

	return service::ModifiedResult<FolderConnection>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Appointment>>> Query::getAppointmentsById(service::FieldParams&&, std::vector<response::IdType>&&) const
{
	throw std::runtime_error(R"ex(Query::getAppointmentsById is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveAppointmentsById(service::ResolverParams&& params)
{
	const auto defaultArguments = []()
	{
		response::Value values(response::Type::Map);
		response::Value entry;

		entry = []()
		{
			response::Value elements(response::Type::List);
			response::Value entry;

			entry = response::Value(std::string(R"gql(ZmFrZUFwcG9pbnRtZW50SWQ=)gql"));
			elements.emplace_back(std::move(entry));
			return elements;
		}();
		values.emplace_back("ids", std::move(entry));

		return values;
	}();

	auto pairIds = service::ModifiedArgument<response::IdType>::find<service::TypeModifier::List>("ids", params.arguments);
	auto argIds = (pairIds.second
		? std::move(pairIds.first)
		: service::ModifiedArgument<response::IdType>::require<service::TypeModifier::List>("ids", defaultArguments));
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getAppointmentsById(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argIds));
	resolverLock.unlock();

	return service::ModifiedResult<Appointment>::convert<service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Task>>> Query::getTasksById(service::FieldParams&&, std::vector<response::IdType>&&) const
{
	throw std::runtime_error(R"ex(Query::getTasksById is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveTasksById(service::ResolverParams&& params)
{
	auto argIds = service::ModifiedArgument<response::IdType>::require<service::TypeModifier::List>("ids", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getTasksById(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argIds));
	resolverLock.unlock();

	return service::ModifiedResult<Task>::convert<service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Folder>>> Query::getUnreadCountsById(service::FieldParams&&, std::vector<response::IdType>&&) const
{
	throw std::runtime_error(R"ex(Query::getUnreadCountsById is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveUnreadCountsById(service::ResolverParams&& params)
{
	auto argIds = service::ModifiedArgument<response::IdType>::require<service::TypeModifier::List>("ids", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getUnreadCountsById(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argIds));
	resolverLock.unlock();

	return service::ModifiedResult<Folder>::convert<service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<NestedType>> Query::getNested(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Query::getNested is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveNested(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNested(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<NestedType>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::StringType> Query::getUnimplemented(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Query::getUnimplemented is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveUnimplemented(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getUnimplemented(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::vector<std::shared_ptr<Expensive>>> Query::getExpensive(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Query::getExpensive is not implemented)ex");
}

std::future<service::ResolverResult> Query::resolveExpensive(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getExpensive(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Expensive>::convert<service::TypeModifier::List>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Query::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Query)gql" }, std::move(params));
}

std::future<service::ResolverResult> Query::resolve_schema(service::ResolverParams&& params)
{
	return service::ModifiedResult<service::Object>::convert(std::static_pointer_cast<service::Object>(std::make_shared<introspection::Schema>(_schema)), std::move(params));
}

std::future<service::ResolverResult> Query::resolve_type(service::ResolverParams&& params)
{
	auto argName = service::ModifiedArgument<response::StringType>::require("name", params.arguments);
	const auto& baseType = _schema->LookupType(argName);
	std::shared_ptr<introspection::object::Type> result { baseType ? std::make_shared<introspection::Type>(baseType) : nullptr };

	return service::ModifiedResult<introspection::object::Type>::convert<service::TypeModifier::Nullable>(result, std::move(params));
}

PageInfo::PageInfo()
	: service::Object({
		"PageInfo"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(hasNextPage)gql"sv, [this](service::ResolverParams&& params) { return resolveHasNextPage(std::move(params)); } },
		{ R"gql(hasPreviousPage)gql"sv, [this](service::ResolverParams&& params) { return resolveHasPreviousPage(std::move(params)); } }
	})
{
}

service::FieldResult<response::BooleanType> PageInfo::getHasNextPage(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(PageInfo::getHasNextPage is not implemented)ex");
}

std::future<service::ResolverResult> PageInfo::resolveHasNextPage(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getHasNextPage(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::BooleanType>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::BooleanType> PageInfo::getHasPreviousPage(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(PageInfo::getHasPreviousPage is not implemented)ex");
}

std::future<service::ResolverResult> PageInfo::resolveHasPreviousPage(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getHasPreviousPage(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::BooleanType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> PageInfo::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(PageInfo)gql" }, std::move(params));
}

AppointmentEdge::AppointmentEdge()
	: service::Object({
		"AppointmentEdge"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(cursor)gql"sv, [this](service::ResolverParams&& params) { return resolveCursor(std::move(params)); } },
		{ R"gql(node)gql"sv, [this](service::ResolverParams&& params) { return resolveNode(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<Appointment>> AppointmentEdge::getNode(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(AppointmentEdge::getNode is not implemented)ex");
}

std::future<service::ResolverResult> AppointmentEdge::resolveNode(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNode(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Appointment>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> AppointmentEdge::getCursor(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(AppointmentEdge::getCursor is not implemented)ex");
}

std::future<service::ResolverResult> AppointmentEdge::resolveCursor(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getCursor(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> AppointmentEdge::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(AppointmentEdge)gql" }, std::move(params));
}

AppointmentConnection::AppointmentConnection()
	: service::Object({
		"AppointmentConnection"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(edges)gql"sv, [this](service::ResolverParams&& params) { return resolveEdges(std::move(params)); } },
		{ R"gql(pageInfo)gql"sv, [this](service::ResolverParams&& params) { return resolvePageInfo(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<PageInfo>> AppointmentConnection::getPageInfo(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(AppointmentConnection::getPageInfo is not implemented)ex");
}

std::future<service::ResolverResult> AppointmentConnection::resolvePageInfo(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getPageInfo(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<PageInfo>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<AppointmentEdge>>>> AppointmentConnection::getEdges(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(AppointmentConnection::getEdges is not implemented)ex");
}

std::future<service::ResolverResult> AppointmentConnection::resolveEdges(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getEdges(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<AppointmentEdge>::convert<service::TypeModifier::Nullable, service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> AppointmentConnection::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(AppointmentConnection)gql" }, std::move(params));
}

TaskEdge::TaskEdge()
	: service::Object({
		"TaskEdge"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(cursor)gql"sv, [this](service::ResolverParams&& params) { return resolveCursor(std::move(params)); } },
		{ R"gql(node)gql"sv, [this](service::ResolverParams&& params) { return resolveNode(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<Task>> TaskEdge::getNode(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(TaskEdge::getNode is not implemented)ex");
}

std::future<service::ResolverResult> TaskEdge::resolveNode(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNode(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Task>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> TaskEdge::getCursor(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(TaskEdge::getCursor is not implemented)ex");
}

std::future<service::ResolverResult> TaskEdge::resolveCursor(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getCursor(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> TaskEdge::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(TaskEdge)gql" }, std::move(params));
}

TaskConnection::TaskConnection()
	: service::Object({
		"TaskConnection"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(edges)gql"sv, [this](service::ResolverParams&& params) { return resolveEdges(std::move(params)); } },
		{ R"gql(pageInfo)gql"sv, [this](service::ResolverParams&& params) { return resolvePageInfo(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<PageInfo>> TaskConnection::getPageInfo(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(TaskConnection::getPageInfo is not implemented)ex");
}

std::future<service::ResolverResult> TaskConnection::resolvePageInfo(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getPageInfo(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<PageInfo>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<TaskEdge>>>> TaskConnection::getEdges(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(TaskConnection::getEdges is not implemented)ex");
}

std::future<service::ResolverResult> TaskConnection::resolveEdges(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getEdges(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<TaskEdge>::convert<service::TypeModifier::Nullable, service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> TaskConnection::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(TaskConnection)gql" }, std::move(params));
}

FolderEdge::FolderEdge()
	: service::Object({
		"FolderEdge"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(cursor)gql"sv, [this](service::ResolverParams&& params) { return resolveCursor(std::move(params)); } },
		{ R"gql(node)gql"sv, [this](service::ResolverParams&& params) { return resolveNode(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<Folder>> FolderEdge::getNode(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(FolderEdge::getNode is not implemented)ex");
}

std::future<service::ResolverResult> FolderEdge::resolveNode(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNode(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Folder>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::Value> FolderEdge::getCursor(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(FolderEdge::getCursor is not implemented)ex");
}

std::future<service::ResolverResult> FolderEdge::resolveCursor(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getCursor(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> FolderEdge::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(FolderEdge)gql" }, std::move(params));
}

FolderConnection::FolderConnection()
	: service::Object({
		"FolderConnection"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(edges)gql"sv, [this](service::ResolverParams&& params) { return resolveEdges(std::move(params)); } },
		{ R"gql(pageInfo)gql"sv, [this](service::ResolverParams&& params) { return resolvePageInfo(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<PageInfo>> FolderConnection::getPageInfo(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(FolderConnection::getPageInfo is not implemented)ex");
}

std::future<service::ResolverResult> FolderConnection::resolvePageInfo(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getPageInfo(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<PageInfo>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<std::vector<std::shared_ptr<FolderEdge>>>> FolderConnection::getEdges(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(FolderConnection::getEdges is not implemented)ex");
}

std::future<service::ResolverResult> FolderConnection::resolveEdges(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getEdges(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<FolderEdge>::convert<service::TypeModifier::Nullable, service::TypeModifier::List, service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> FolderConnection::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(FolderConnection)gql" }, std::move(params));
}

CompleteTaskPayload::CompleteTaskPayload()
	: service::Object({
		"CompleteTaskPayload"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(clientMutationId)gql"sv, [this](service::ResolverParams&& params) { return resolveClientMutationId(std::move(params)); } },
		{ R"gql(task)gql"sv, [this](service::ResolverParams&& params) { return resolveTask(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<Task>> CompleteTaskPayload::getTask(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(CompleteTaskPayload::getTask is not implemented)ex");
}

std::future<service::ResolverResult> CompleteTaskPayload::resolveTask(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getTask(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Task>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::StringType>> CompleteTaskPayload::getClientMutationId(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(CompleteTaskPayload::getClientMutationId is not implemented)ex");
}

std::future<service::ResolverResult> CompleteTaskPayload::resolveClientMutationId(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getClientMutationId(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> CompleteTaskPayload::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(CompleteTaskPayload)gql" }, std::move(params));
}

Mutation::Mutation()
	: service::Object({
		"Mutation"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(completeTask)gql"sv, [this](service::ResolverParams&& params) { return resolveCompleteTask(std::move(params)); } },
		{ R"gql(setFloat)gql"sv, [this](service::ResolverParams&& params) { return resolveSetFloat(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<CompleteTaskPayload>> Mutation::applyCompleteTask(service::FieldParams&&, CompleteTaskInput&&) const
{
	throw std::runtime_error(R"ex(Mutation::applyCompleteTask is not implemented)ex");
}

std::future<service::ResolverResult> Mutation::resolveCompleteTask(service::ResolverParams&& params)
{
	auto argInput = service::ModifiedArgument<today::CompleteTaskInput>::require("input", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = applyCompleteTask(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argInput));
	resolverLock.unlock();

	return service::ModifiedResult<CompleteTaskPayload>::convert(std::move(result), std::move(params));
}

service::FieldResult<response::FloatType> Mutation::applySetFloat(service::FieldParams&&, response::FloatType&&) const
{
	throw std::runtime_error(R"ex(Mutation::applySetFloat is not implemented)ex");
}

std::future<service::ResolverResult> Mutation::resolveSetFloat(service::ResolverParams&& params)
{
	auto argValue = service::ModifiedArgument<response::FloatType>::require("value", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = applySetFloat(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argValue));
	resolverLock.unlock();

	return service::ModifiedResult<response::FloatType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Mutation::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Mutation)gql" }, std::move(params));
}

Subscription::Subscription()
	: service::Object({
		"Subscription"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(nextAppointmentChange)gql"sv, [this](service::ResolverParams&& params) { return resolveNextAppointmentChange(std::move(params)); } },
		{ R"gql(nodeChange)gql"sv, [this](service::ResolverParams&& params) { return resolveNodeChange(std::move(params)); } }
	})
{
}

service::FieldResult<std::shared_ptr<Appointment>> Subscription::getNextAppointmentChange(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Subscription::getNextAppointmentChange is not implemented)ex");
}

std::future<service::ResolverResult> Subscription::resolveNextAppointmentChange(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNextAppointmentChange(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<Appointment>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<service::Object>> Subscription::getNodeChange(service::FieldParams&&, response::IdType&&) const
{
	throw std::runtime_error(R"ex(Subscription::getNodeChange is not implemented)ex");
}

std::future<service::ResolverResult> Subscription::resolveNodeChange(service::ResolverParams&& params)
{
	auto argId = service::ModifiedArgument<response::IdType>::require("id", params.arguments);
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNodeChange(service::FieldParams(params, std::move(params.fieldDirectives)), std::move(argId));
	resolverLock.unlock();

	return service::ModifiedResult<service::Object>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Subscription::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Subscription)gql" }, std::move(params));
}

Appointment::Appointment()
	: service::Object({
		"Node",
		"UnionType",
		"Appointment"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(forceError)gql"sv, [this](service::ResolverParams&& params) { return resolveForceError(std::move(params)); } },
		{ R"gql(id)gql"sv, [this](service::ResolverParams&& params) { return resolveId(std::move(params)); } },
		{ R"gql(isNow)gql"sv, [this](service::ResolverParams&& params) { return resolveIsNow(std::move(params)); } },
		{ R"gql(subject)gql"sv, [this](service::ResolverParams&& params) { return resolveSubject(std::move(params)); } },
		{ R"gql(when)gql"sv, [this](service::ResolverParams&& params) { return resolveWhen(std::move(params)); } }
	})
{
}

service::FieldResult<response::IdType> Appointment::getId(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Appointment::getId is not implemented)ex");
}

std::future<service::ResolverResult> Appointment::resolveId(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getId(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IdType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::Value>> Appointment::getWhen(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Appointment::getWhen is not implemented)ex");
}

std::future<service::ResolverResult> Appointment::resolveWhen(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getWhen(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::Value>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::StringType>> Appointment::getSubject(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Appointment::getSubject is not implemented)ex");
}

std::future<service::ResolverResult> Appointment::resolveSubject(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getSubject(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::BooleanType> Appointment::getIsNow(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Appointment::getIsNow is not implemented)ex");
}

std::future<service::ResolverResult> Appointment::resolveIsNow(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getIsNow(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::BooleanType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::StringType>> Appointment::getForceError(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Appointment::getForceError is not implemented)ex");
}

std::future<service::ResolverResult> Appointment::resolveForceError(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getForceError(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Appointment::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Appointment)gql" }, std::move(params));
}

Task::Task()
	: service::Object({
		"Node",
		"UnionType",
		"Task"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(id)gql"sv, [this](service::ResolverParams&& params) { return resolveId(std::move(params)); } },
		{ R"gql(isComplete)gql"sv, [this](service::ResolverParams&& params) { return resolveIsComplete(std::move(params)); } },
		{ R"gql(title)gql"sv, [this](service::ResolverParams&& params) { return resolveTitle(std::move(params)); } }
	})
{
}

service::FieldResult<response::IdType> Task::getId(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Task::getId is not implemented)ex");
}

std::future<service::ResolverResult> Task::resolveId(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getId(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IdType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::StringType>> Task::getTitle(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Task::getTitle is not implemented)ex");
}

std::future<service::ResolverResult> Task::resolveTitle(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getTitle(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::BooleanType> Task::getIsComplete(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Task::getIsComplete is not implemented)ex");
}

std::future<service::ResolverResult> Task::resolveIsComplete(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getIsComplete(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::BooleanType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Task::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Task)gql" }, std::move(params));
}

Folder::Folder()
	: service::Object({
		"Node",
		"UnionType",
		"Folder"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(id)gql"sv, [this](service::ResolverParams&& params) { return resolveId(std::move(params)); } },
		{ R"gql(name)gql"sv, [this](service::ResolverParams&& params) { return resolveName(std::move(params)); } },
		{ R"gql(unreadCount)gql"sv, [this](service::ResolverParams&& params) { return resolveUnreadCount(std::move(params)); } }
	})
{
}

service::FieldResult<response::IdType> Folder::getId(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Folder::getId is not implemented)ex");
}

std::future<service::ResolverResult> Folder::resolveId(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getId(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IdType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::optional<response::StringType>> Folder::getName(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Folder::getName is not implemented)ex");
}

std::future<service::ResolverResult> Folder::resolveName(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getName(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::StringType>::convert<service::TypeModifier::Nullable>(std::move(result), std::move(params));
}

service::FieldResult<response::IntType> Folder::getUnreadCount(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Folder::getUnreadCount is not implemented)ex");
}

std::future<service::ResolverResult> Folder::resolveUnreadCount(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getUnreadCount(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Folder::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Folder)gql" }, std::move(params));
}

NestedType::NestedType()
	: service::Object({
		"NestedType"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(depth)gql"sv, [this](service::ResolverParams&& params) { return resolveDepth(std::move(params)); } },
		{ R"gql(nested)gql"sv, [this](service::ResolverParams&& params) { return resolveNested(std::move(params)); } }
	})
{
}

service::FieldResult<response::IntType> NestedType::getDepth(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(NestedType::getDepth is not implemented)ex");
}

std::future<service::ResolverResult> NestedType::resolveDepth(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getDepth(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

service::FieldResult<std::shared_ptr<NestedType>> NestedType::getNested(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(NestedType::getNested is not implemented)ex");
}

std::future<service::ResolverResult> NestedType::resolveNested(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getNested(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<NestedType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> NestedType::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(NestedType)gql" }, std::move(params));
}

Expensive::Expensive()
	: service::Object({
		"Expensive"
	}, {
		{ R"gql(__typename)gql"sv, [this](service::ResolverParams&& params) { return resolve_typename(std::move(params)); } },
		{ R"gql(order)gql"sv, [this](service::ResolverParams&& params) { return resolveOrder(std::move(params)); } }
	})
{
}

service::FieldResult<response::IntType> Expensive::getOrder(service::FieldParams&&) const
{
	throw std::runtime_error(R"ex(Expensive::getOrder is not implemented)ex");
}

std::future<service::ResolverResult> Expensive::resolveOrder(service::ResolverParams&& params)
{
	std::unique_lock resolverLock(_resolverMutex);
	auto result = getOrder(service::FieldParams(params, std::move(params.fieldDirectives)));
	resolverLock.unlock();

	return service::ModifiedResult<response::IntType>::convert(std::move(result), std::move(params));
}

std::future<service::ResolverResult> Expensive::resolve_typename(service::ResolverParams&& params)
{
	return service::ModifiedResult<response::StringType>::convert(response::StringType{ R"gql(Expensive)gql" }, std::move(params));
}

} /* namespace object */

Operations::Operations(std::shared_ptr<object::Query> query, std::shared_ptr<object::Mutation> mutation, std::shared_ptr<object::Subscription> subscription)
	: service::Request({
		{ "query", query },
		{ "mutation", mutation },
		{ "subscription", subscription }
	}, GetSchema())
	, _query(std::move(query))
	, _mutation(std::move(mutation))
	, _subscription(std::move(subscription))
{
}

void AddTypesToSchema(const std::shared_ptr<schema::Schema>& schema)
{
	schema->AddType(R"gql(ItemCursor)gql"sv, schema::ScalarType::Make(R"gql(ItemCursor)gql"sv, R"md()md"));
	schema->AddType(R"gql(DateTime)gql"sv, schema::ScalarType::Make(R"gql(DateTime)gql"sv, R"md()md"));
	auto typeTaskState = schema::EnumType::Make(R"gql(TaskState)gql"sv, R"md()md"sv);
	schema->AddType(R"gql(TaskState)gql"sv, typeTaskState);
	auto typeCompleteTaskInput = schema::InputObjectType::Make(R"gql(CompleteTaskInput)gql"sv, R"md()md"sv);
	schema->AddType(R"gql(CompleteTaskInput)gql"sv, typeCompleteTaskInput);
	auto typeUnionType = schema::UnionType::Make(R"gql(UnionType)gql"sv, R"md()md"sv);
	schema->AddType(R"gql(UnionType)gql"sv, typeUnionType);
	auto typeNode = schema::InterfaceType::Make(R"gql(Node)gql"sv, R"md(Node interface for Relay support)md"sv);
	schema->AddType(R"gql(Node)gql"sv, typeNode);
	auto typeQuery = schema::ObjectType::Make(R"gql(Query)gql"sv, R"md(Root Query type)md");
	schema->AddType(R"gql(Query)gql"sv, typeQuery);
	auto typePageInfo = schema::ObjectType::Make(R"gql(PageInfo)gql"sv, R"md()md");
	schema->AddType(R"gql(PageInfo)gql"sv, typePageInfo);
	auto typeAppointmentEdge = schema::ObjectType::Make(R"gql(AppointmentEdge)gql"sv, R"md()md");
	schema->AddType(R"gql(AppointmentEdge)gql"sv, typeAppointmentEdge);
	auto typeAppointmentConnection = schema::ObjectType::Make(R"gql(AppointmentConnection)gql"sv, R"md()md");
	schema->AddType(R"gql(AppointmentConnection)gql"sv, typeAppointmentConnection);
	auto typeTaskEdge = schema::ObjectType::Make(R"gql(TaskEdge)gql"sv, R"md()md");
	schema->AddType(R"gql(TaskEdge)gql"sv, typeTaskEdge);
	auto typeTaskConnection = schema::ObjectType::Make(R"gql(TaskConnection)gql"sv, R"md()md");
	schema->AddType(R"gql(TaskConnection)gql"sv, typeTaskConnection);
	auto typeFolderEdge = schema::ObjectType::Make(R"gql(FolderEdge)gql"sv, R"md()md");
	schema->AddType(R"gql(FolderEdge)gql"sv, typeFolderEdge);
	auto typeFolderConnection = schema::ObjectType::Make(R"gql(FolderConnection)gql"sv, R"md()md");
	schema->AddType(R"gql(FolderConnection)gql"sv, typeFolderConnection);
	auto typeCompleteTaskPayload = schema::ObjectType::Make(R"gql(CompleteTaskPayload)gql"sv, R"md()md");
	schema->AddType(R"gql(CompleteTaskPayload)gql"sv, typeCompleteTaskPayload);
	auto typeMutation = schema::ObjectType::Make(R"gql(Mutation)gql"sv, R"md()md");
	schema->AddType(R"gql(Mutation)gql"sv, typeMutation);
	auto typeSubscription = schema::ObjectType::Make(R"gql(Subscription)gql"sv, R"md()md");
	schema->AddType(R"gql(Subscription)gql"sv, typeSubscription);
	auto typeAppointment = schema::ObjectType::Make(R"gql(Appointment)gql"sv, R"md()md");
	schema->AddType(R"gql(Appointment)gql"sv, typeAppointment);
	auto typeTask = schema::ObjectType::Make(R"gql(Task)gql"sv, R"md()md");
	schema->AddType(R"gql(Task)gql"sv, typeTask);
	auto typeFolder = schema::ObjectType::Make(R"gql(Folder)gql"sv, R"md()md");
	schema->AddType(R"gql(Folder)gql"sv, typeFolder);
	auto typeNestedType = schema::ObjectType::Make(R"gql(NestedType)gql"sv, R"md(Infinitely nestable type which can be used with nested fragments to test directive handling)md");
	schema->AddType(R"gql(NestedType)gql"sv, typeNestedType);
	auto typeExpensive = schema::ObjectType::Make(R"gql(Expensive)gql"sv, R"md()md");
	schema->AddType(R"gql(Expensive)gql"sv, typeExpensive);

	typeTaskState->AddEnumValues({
		{ service::s_namesTaskState[static_cast<size_t>(today::TaskState::New)], R"md()md"sv, std::nullopt },
		{ service::s_namesTaskState[static_cast<size_t>(today::TaskState::Started)], R"md()md"sv, std::nullopt },
		{ service::s_namesTaskState[static_cast<size_t>(today::TaskState::Complete)], R"md()md"sv, std::nullopt },
		{ service::s_namesTaskState[static_cast<size_t>(today::TaskState::Unassigned)], R"md()md"sv, std::make_optional(R"md(Need to deprecate an [enum value](https://facebook.github.io/graphql/June2018/#sec-Deprecation))md"sv) }
	});

	typeCompleteTaskInput->AddInputValues({
		schema::InputValue::Make(R"gql(id)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")), R"gql()gql"sv),
		schema::InputValue::Make(R"gql(isComplete)gql"sv, R"md()md"sv, schema->LookupType("Boolean"), R"gql(true)gql"sv),
		schema::InputValue::Make(R"gql(clientMutationId)gql"sv, R"md()md"sv, schema->LookupType("String"), R"gql()gql"sv)
	});

	typeUnionType->AddPossibleTypes({
		schema->LookupType(R"gql(Appointment)gql"sv),
		schema->LookupType(R"gql(Task)gql"sv),
		schema->LookupType(R"gql(Folder)gql"sv)
	});

	typeNode->AddFields({
		schema::Field::Make(R"gql(id)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")))
	});

	typeQuery->AddFields({
		schema::Field::Make(R"gql(node)gql"sv, R"md([Object Identification](https://facebook.github.io/relay/docs/en/graphql-server-specification.html#object-identification))md"sv, std::nullopt, schema->LookupType("Node"), {
			schema::InputValue::Make(R"gql(id)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(appointments)gql"sv, R"md(Appointments [Connection](https://facebook.github.io/relay/docs/en/graphql-server-specification.html#connections))md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("AppointmentConnection")), {
			schema::InputValue::Make(R"gql(first)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(after)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(last)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(before)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(tasks)gql"sv, R"md(Tasks [Connection](https://facebook.github.io/relay/docs/en/graphql-server-specification.html#connections))md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("TaskConnection")), {
			schema::InputValue::Make(R"gql(first)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(after)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(last)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(before)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(unreadCounts)gql"sv, R"md(Folder unread counts [Connection](https://facebook.github.io/relay/docs/en/graphql-server-specification.html#connections))md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("FolderConnection")), {
			schema::InputValue::Make(R"gql(first)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(after)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(last)gql"sv, R"md()md"sv, schema->LookupType("Int"), R"gql()gql"sv),
			schema::InputValue::Make(R"gql(before)gql"sv, R"md()md"sv, schema->LookupType("ItemCursor"), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(appointmentsById)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("Appointment"))), {
			schema::InputValue::Make(R"gql(ids)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")))), R"gql(["ZmFrZUFwcG9pbnRtZW50SWQ="])gql"sv)
		}),
		schema::Field::Make(R"gql(tasksById)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("Task"))), {
			schema::InputValue::Make(R"gql(ids)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")))), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(unreadCountsById)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("Folder"))), {
			schema::InputValue::Make(R"gql(ids)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")))), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(nested)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("NestedType"))),
		schema::Field::Make(R"gql(unimplemented)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String"))),
		schema::Field::Make(R"gql(expensive)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->WrapType(introspection::TypeKind::LIST, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Expensive")))))
	});
	typePageInfo->AddFields({
		schema::Field::Make(R"gql(hasNextPage)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Boolean"))),
		schema::Field::Make(R"gql(hasPreviousPage)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Boolean")))
	});
	typeAppointmentEdge->AddFields({
		schema::Field::Make(R"gql(node)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Appointment")),
		schema::Field::Make(R"gql(cursor)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ItemCursor")))
	});
	typeAppointmentConnection->AddFields({
		schema::Field::Make(R"gql(pageInfo)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("PageInfo"))),
		schema::Field::Make(R"gql(edges)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("AppointmentEdge")))
	});
	typeTaskEdge->AddFields({
		schema::Field::Make(R"gql(node)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Task")),
		schema::Field::Make(R"gql(cursor)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ItemCursor")))
	});
	typeTaskConnection->AddFields({
		schema::Field::Make(R"gql(pageInfo)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("PageInfo"))),
		schema::Field::Make(R"gql(edges)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("TaskEdge")))
	});
	typeFolderEdge->AddFields({
		schema::Field::Make(R"gql(node)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Folder")),
		schema::Field::Make(R"gql(cursor)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ItemCursor")))
	});
	typeFolderConnection->AddFields({
		schema::Field::Make(R"gql(pageInfo)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("PageInfo"))),
		schema::Field::Make(R"gql(edges)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::LIST, schema->LookupType("FolderEdge")))
	});
	typeCompleteTaskPayload->AddFields({
		schema::Field::Make(R"gql(task)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("Task")),
		schema::Field::Make(R"gql(clientMutationId)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("String"))
	});
	typeMutation->AddFields({
		schema::Field::Make(R"gql(completeTask)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CompleteTaskPayload")), {
			schema::InputValue::Make(R"gql(input)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("CompleteTaskInput")), R"gql()gql"sv)
		}),
		schema::Field::Make(R"gql(setFloat)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Float")), {
			schema::InputValue::Make(R"gql(value)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Float")), R"gql()gql"sv)
		})
	});
	typeSubscription->AddFields({
		schema::Field::Make(R"gql(nextAppointmentChange)gql"sv, R"md()md"sv, std::make_optional(R"md(Need to deprecate a [field](https://facebook.github.io/graphql/June2018/#sec-Deprecation))md"sv), schema->LookupType("Appointment")),
		schema::Field::Make(R"gql(nodeChange)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Node")), {
			schema::InputValue::Make(R"gql(id)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID")), R"gql()gql"sv)
		})
	});
	typeAppointment->AddInterfaces({
		typeNode
	});
	typeAppointment->AddFields({
		schema::Field::Make(R"gql(id)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID"))),
		schema::Field::Make(R"gql(when)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("DateTime")),
		schema::Field::Make(R"gql(subject)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("String")),
		schema::Field::Make(R"gql(isNow)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Boolean"))),
		schema::Field::Make(R"gql(forceError)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("String"))
	});
	typeTask->AddInterfaces({
		typeNode
	});
	typeTask->AddFields({
		schema::Field::Make(R"gql(id)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID"))),
		schema::Field::Make(R"gql(title)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("String")),
		schema::Field::Make(R"gql(isComplete)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Boolean")))
	});
	typeFolder->AddInterfaces({
		typeNode
	});
	typeFolder->AddFields({
		schema::Field::Make(R"gql(id)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("ID"))),
		schema::Field::Make(R"gql(name)gql"sv, R"md()md"sv, std::nullopt, schema->LookupType("String")),
		schema::Field::Make(R"gql(unreadCount)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int")))
	});
	typeNestedType->AddFields({
		schema::Field::Make(R"gql(depth)gql"sv, R"md(Depth of the nested element)md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int"))),
		schema::Field::Make(R"gql(nested)gql"sv, R"md(Link to the next level)md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("NestedType")))
	});
	typeExpensive->AddFields({
		schema::Field::Make(R"gql(order)gql"sv, R"md()md"sv, std::nullopt, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("Int")))
	});

	schema->AddDirective(schema::Directive::Make(R"gql(id)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::FIELD_DEFINITION
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(subscriptionTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::SUBSCRIPTION
	}, {
		schema::InputValue::Make(R"gql(field)gql"sv, R"md()md"sv, schema->LookupType("String"), R"gql()gql"sv)
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(queryTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::QUERY
	}, {
		schema::InputValue::Make(R"gql(query)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String")), R"gql()gql"sv)
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(fieldTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::FIELD
	}, {
		schema::InputValue::Make(R"gql(field)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String")), R"gql()gql"sv)
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(fragmentDefinitionTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::FRAGMENT_DEFINITION
	}, {
		schema::InputValue::Make(R"gql(fragmentDefinition)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String")), R"gql()gql"sv)
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(fragmentSpreadTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::FRAGMENT_SPREAD
	}, {
		schema::InputValue::Make(R"gql(fragmentSpread)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String")), R"gql()gql"sv)
	}));
	schema->AddDirective(schema::Directive::Make(R"gql(inlineFragmentTag)gql"sv, R"md()md"sv, {
		introspection::DirectiveLocation::INLINE_FRAGMENT
	}, {
		schema::InputValue::Make(R"gql(inlineFragment)gql"sv, R"md()md"sv, schema->WrapType(introspection::TypeKind::NON_NULL, schema->LookupType("String")), R"gql()gql"sv)
	}));

	schema->AddQueryType(typeQuery);
	schema->AddMutationType(typeMutation);
	schema->AddSubscriptionType(typeSubscription);
}

std::shared_ptr<schema::Schema> GetSchema()
{
	static std::weak_ptr<schema::Schema> s_wpSchema;
	auto schema = s_wpSchema.lock();

	if (!schema)
	{
		schema = std::make_shared<schema::Schema>(false);
		introspection::AddTypesToSchema(schema);
		AddTypesToSchema(schema);
		s_wpSchema = schema;
	}

	return schema;
}

} /* namespace today */
} /* namespace graphql */
