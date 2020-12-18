// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef TODAYSCHEMA_H
#define TODAYSCHEMA_H

#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"

#include <memory>
#include <string>
#include <vector>

namespace graphql {
namespace today {

enum class TaskState
{
	New,
	Started,
	Complete,
	Unassigned
};

struct CompleteTaskInput
{
	response::IdType id;
	std::optional<response::BooleanType> isComplete;
	std::optional<response::StringType> clientMutationId;
};

namespace object {

class Query;
class PageInfo;
class AppointmentEdge;
class AppointmentConnection;
class TaskEdge;
class TaskConnection;
class FolderEdge;
class FolderConnection;
class CompleteTaskPayload;
class Mutation;
class Subscription;
class Appointment;
class Task;
class Folder;
class NestedType;
class Expensive;

} /* namespace object */

struct Node
{
	virtual service::FieldResult<response::IdType> getId(service::FieldParams&& params) const = 0;
};

namespace object {

class Query
	: public service::Object
{
protected:
	explicit Query();

public:
	virtual service::FieldResult<std::shared_ptr<service::Object>> getNode(service::FieldParams&& params, response::IdType&& idArg) const;
	virtual service::FieldResult<std::shared_ptr<AppointmentConnection>> getAppointments(service::FieldParams&& params, std::optional<response::IntType>&& firstArg, std::optional<response::Value>&& afterArg, std::optional<response::IntType>&& lastArg, std::optional<response::Value>&& beforeArg) const;
	virtual service::FieldResult<std::shared_ptr<TaskConnection>> getTasks(service::FieldParams&& params, std::optional<response::IntType>&& firstArg, std::optional<response::Value>&& afterArg, std::optional<response::IntType>&& lastArg, std::optional<response::Value>&& beforeArg) const;
	virtual service::FieldResult<std::shared_ptr<FolderConnection>> getUnreadCounts(service::FieldParams&& params, std::optional<response::IntType>&& firstArg, std::optional<response::Value>&& afterArg, std::optional<response::IntType>&& lastArg, std::optional<response::Value>&& beforeArg) const;
	virtual service::FieldResult<std::vector<std::shared_ptr<Appointment>>> getAppointmentsById(service::FieldParams&& params, std::vector<response::IdType>&& idsArg) const;
	virtual service::FieldResult<std::vector<std::shared_ptr<Task>>> getTasksById(service::FieldParams&& params, std::vector<response::IdType>&& idsArg) const;
	virtual service::FieldResult<std::vector<std::shared_ptr<Folder>>> getUnreadCountsById(service::FieldParams&& params, std::vector<response::IdType>&& idsArg) const;
	virtual service::FieldResult<std::shared_ptr<NestedType>> getNested(service::FieldParams&& params) const;
	virtual service::FieldResult<response::StringType> getUnimplemented(service::FieldParams&& params) const;
	virtual service::FieldResult<std::vector<std::shared_ptr<Expensive>>> getExpensive(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveNode(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveAppointments(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveTasks(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveUnreadCounts(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveAppointmentsById(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveTasksById(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveUnreadCountsById(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveNested(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveUnimplemented(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveExpensive(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolve_schema(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolve_type(service::ResolverParams&& params);

	std::shared_ptr<schema::Schema> _schema;
};

class PageInfo
	: public service::Object
{
protected:
	explicit PageInfo();

public:
	virtual service::FieldResult<response::BooleanType> getHasNextPage(service::FieldParams&& params) const;
	virtual service::FieldResult<response::BooleanType> getHasPreviousPage(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveHasNextPage(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveHasPreviousPage(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class AppointmentEdge
	: public service::Object
{
protected:
	explicit AppointmentEdge();

public:
	virtual service::FieldResult<std::shared_ptr<Appointment>> getNode(service::FieldParams&& params) const;
	virtual service::FieldResult<response::Value> getCursor(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveNode(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveCursor(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class AppointmentConnection
	: public service::Object
{
protected:
	explicit AppointmentConnection();

public:
	virtual service::FieldResult<std::shared_ptr<PageInfo>> getPageInfo(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<AppointmentEdge>>>> getEdges(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolvePageInfo(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveEdges(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class TaskEdge
	: public service::Object
{
protected:
	explicit TaskEdge();

public:
	virtual service::FieldResult<std::shared_ptr<Task>> getNode(service::FieldParams&& params) const;
	virtual service::FieldResult<response::Value> getCursor(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveNode(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveCursor(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class TaskConnection
	: public service::Object
{
protected:
	explicit TaskConnection();

public:
	virtual service::FieldResult<std::shared_ptr<PageInfo>> getPageInfo(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<TaskEdge>>>> getEdges(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolvePageInfo(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveEdges(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class FolderEdge
	: public service::Object
{
protected:
	explicit FolderEdge();

public:
	virtual service::FieldResult<std::shared_ptr<Folder>> getNode(service::FieldParams&& params) const;
	virtual service::FieldResult<response::Value> getCursor(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveNode(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveCursor(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class FolderConnection
	: public service::Object
{
protected:
	explicit FolderConnection();

public:
	virtual service::FieldResult<std::shared_ptr<PageInfo>> getPageInfo(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<std::vector<std::shared_ptr<FolderEdge>>>> getEdges(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolvePageInfo(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveEdges(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class CompleteTaskPayload
	: public service::Object
{
protected:
	explicit CompleteTaskPayload();

public:
	virtual service::FieldResult<std::shared_ptr<Task>> getTask(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<response::StringType>> getClientMutationId(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveTask(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveClientMutationId(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Mutation
	: public service::Object
{
protected:
	explicit Mutation();

public:
	virtual service::FieldResult<std::shared_ptr<CompleteTaskPayload>> applyCompleteTask(service::FieldParams&& params, CompleteTaskInput&& inputArg) const;
	virtual service::FieldResult<response::FloatType> applySetFloat(service::FieldParams&& params, response::FloatType&& valueArg) const;

private:
	std::future<service::ResolverResult> resolveCompleteTask(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveSetFloat(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Subscription
	: public service::Object
{
protected:
	explicit Subscription();

public:
	virtual service::FieldResult<std::shared_ptr<Appointment>> getNextAppointmentChange(service::FieldParams&& params) const;
	virtual service::FieldResult<std::shared_ptr<service::Object>> getNodeChange(service::FieldParams&& params, response::IdType&& idArg) const;

private:
	std::future<service::ResolverResult> resolveNextAppointmentChange(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveNodeChange(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Appointment
	: public service::Object
	, public Node
{
protected:
	explicit Appointment();

public:
	virtual service::FieldResult<response::IdType> getId(service::FieldParams&& params) const override;
	virtual service::FieldResult<std::optional<response::Value>> getWhen(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<response::StringType>> getSubject(service::FieldParams&& params) const;
	virtual service::FieldResult<response::BooleanType> getIsNow(service::FieldParams&& params) const;
	virtual service::FieldResult<std::optional<response::StringType>> getForceError(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveId(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveWhen(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveSubject(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveIsNow(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveForceError(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Task
	: public service::Object
	, public Node
{
protected:
	explicit Task();

public:
	virtual service::FieldResult<response::IdType> getId(service::FieldParams&& params) const override;
	virtual service::FieldResult<std::optional<response::StringType>> getTitle(service::FieldParams&& params) const;
	virtual service::FieldResult<response::BooleanType> getIsComplete(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveId(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveTitle(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveIsComplete(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Folder
	: public service::Object
	, public Node
{
protected:
	explicit Folder();

public:
	virtual service::FieldResult<response::IdType> getId(service::FieldParams&& params) const override;
	virtual service::FieldResult<std::optional<response::StringType>> getName(service::FieldParams&& params) const;
	virtual service::FieldResult<response::IntType> getUnreadCount(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveId(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveName(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveUnreadCount(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class NestedType
	: public service::Object
{
protected:
	explicit NestedType();

public:
	virtual service::FieldResult<response::IntType> getDepth(service::FieldParams&& params) const;
	virtual service::FieldResult<std::shared_ptr<NestedType>> getNested(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveDepth(service::ResolverParams&& params);
	std::future<service::ResolverResult> resolveNested(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

class Expensive
	: public service::Object
{
protected:
	explicit Expensive();

public:
	virtual service::FieldResult<response::IntType> getOrder(service::FieldParams&& params) const;

private:
	std::future<service::ResolverResult> resolveOrder(service::ResolverParams&& params);

	std::future<service::ResolverResult> resolve_typename(service::ResolverParams&& params);
};

} /* namespace object */

class Operations
	: public service::Request
{
public:
	explicit Operations(std::shared_ptr<object::Query> query, std::shared_ptr<object::Mutation> mutation, std::shared_ptr<object::Subscription> subscription);

private:
	std::shared_ptr<object::Query> _query;
	std::shared_ptr<object::Mutation> _mutation;
	std::shared_ptr<object::Subscription> _subscription;
};

std::shared_ptr<schema::Schema> GetSchema();

} /* namespace today */
} /* namespace graphql */

#endif // TODAYSCHEMA_H
