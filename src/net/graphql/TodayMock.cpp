// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "TodayMock.h"

#include <algorithm>
#include <iostream>

namespace graphql::today {

Appointment::Appointment(
	response::IdType&& id, std::string&& when, std::string&& subject, bool isNow)
	: _id(std::move(id))
	, _when(std::move(when))
	, _subject(std::move(subject))
	, _isNow(isNow)
{
}

Task::Task(response::IdType&& id, std::string&& title, bool isComplete)
	: _id(std::move(id))
	, _title(std::move(title))
	, _isComplete(isComplete)
{
}

Folder::Folder(response::IdType&& id, std::string&& name, int unreadCount)
	: _id(std::move(id))
	, _name(std::move(name))
	, _unreadCount(unreadCount)
{
}

Query::Query(appointmentsLoader&& getAppointments, tasksLoader&& getTasks,
	unreadCountsLoader&& getUnreadCounts)
	: _getAppointments(std::move(getAppointments))
	, _getTasks(std::move(getTasks))
	, _getUnreadCounts(getUnreadCounts)
{
}

void Query::loadAppointments(const std::shared_ptr<service::RequestState>& state) const
{
	if (state)
	{
		auto todayState = std::static_pointer_cast<RequestState>(state);

		todayState->appointmentsRequestId = todayState->requestId;
		todayState->loadAppointmentsCount++;
	}

	if (_getAppointments)
	{
		_appointments = _getAppointments();
		_getAppointments = nullptr;
	}
}

std::shared_ptr<Appointment> Query::findAppointment(
	const service::FieldParams& params, const response::IdType& id) const
{
	loadAppointments(params.state);

	for (const auto& appointment : _appointments)
	{
		auto appointmentId =
			appointment->getId(service::FieldParams(params, response::Value(response::Type::Map)))
				.get();

		if (appointmentId == id)
		{
			return appointment;
		}
	}

	return nullptr;
}

void Query::loadTasks(const std::shared_ptr<service::RequestState>& state) const
{
	if (state)
	{
		auto todayState = std::static_pointer_cast<RequestState>(state);

		todayState->tasksRequestId = todayState->requestId;
		todayState->loadTasksCount++;
	}

	if (_getTasks)
	{
		_tasks = _getTasks();
		_getTasks = nullptr;
	}
}

std::shared_ptr<Task> Query::findTask(
	const service::FieldParams& params, const response::IdType& id) const
{
	loadTasks(params.state);

	for (const auto& task : _tasks)
	{
		auto taskId =
			task->getId(service::FieldParams(params, response::Value(response::Type::Map))).get();

		if (taskId == id)
		{
			return task;
		}
	}

	return nullptr;
}

void Query::loadUnreadCounts(const std::shared_ptr<service::RequestState>& state) const
{
	if (state)
	{
		auto todayState = std::static_pointer_cast<RequestState>(state);

		todayState->unreadCountsRequestId = todayState->requestId;
		todayState->loadUnreadCountsCount++;
	}

	if (_getUnreadCounts)
	{
		_unreadCounts = _getUnreadCounts();
		_getUnreadCounts = nullptr;
	}
}

std::shared_ptr<Folder> Query::findUnreadCount(
	const service::FieldParams& params, const response::IdType& id) const
{
	loadUnreadCounts(params.state);

	for (const auto& folder : _unreadCounts)
	{
		auto folderId =
			folder->getId(service::FieldParams(params, response::Value(response::Type::Map))).get();

		if (folderId == id)
		{
			return folder;
		}
	}

	return nullptr;
}

service::FieldResult<std::shared_ptr<service::Object>> Query::getNode(
	service::FieldParams&& params, response::IdType&& id) const
{
	std::promise<std::shared_ptr<service::Object>> promise;
	auto appointment = findAppointment(params, id);

	if (appointment)
	{
		promise.set_value(appointment);
		return promise.get_future();
	}

	auto task = findTask(params, id);

	if (task)
	{
		promise.set_value(task);
		return promise.get_future();
	}

	auto folder = findUnreadCount(params, id);

	if (folder)
	{
		promise.set_value(folder);
		return promise.get_future();
	}

	promise.set_value(nullptr);
	return promise.get_future();
}

template <class _Object, class _Connection>
struct EdgeConstraints
{
	using vec_type = std::vector<std::shared_ptr<_Object>>;
	using itr_type = typename vec_type::const_iterator;

	EdgeConstraints(const std::shared_ptr<service::RequestState>& state, service::field_path&& path,
		const vec_type& objects)
		: _state(state)
		, _path(std::move(path))
		, _objects(objects)
	{
	}

	std::shared_ptr<_Connection> operator()(const std::optional<int>& first,
		const std::optional<response::Value>& after, const std::optional<int>& last,
		const std::optional<response::Value>& before) const
	{
		auto itrFirst = _objects.cbegin();
		auto itrLast = _objects.cend();

		if (after)
		{
			const auto& encoded = after->get<response::StringType>();
			auto afterId = service::Base64::fromBase64(encoded.c_str(), encoded.size());
			auto itrAfter =
				std::find_if(itrFirst, itrLast, [&afterId](const std::shared_ptr<_Object>& entry) {
					return entry->id() == afterId;
				});

			if (itrAfter != itrLast)
			{
				itrFirst = itrAfter;
			}
		}

		if (before)
		{
			const auto& encoded = before->get<response::StringType>();
			auto beforeId = service::Base64::fromBase64(encoded.c_str(), encoded.size());
			auto itrBefore =
				std::find_if(itrFirst, itrLast, [&beforeId](const std::shared_ptr<_Object>& entry) {
					return entry->id() == beforeId;
				});

			if (itrBefore != itrLast)
			{
				itrLast = itrBefore;
				++itrLast;
			}
		}

		if (first)
		{
			if (*first < 0)
			{
				std::ostringstream error;

				error << "Invalid argument: first value: " << *first;
				throw service::schema_exception {
					{ service::schema_error { error.str(), {}, _path } }
				};
			}

			if (itrLast - itrFirst > *first)
			{
				itrLast = itrFirst + *first;
			}
		}

		if (last)
		{
			if (*last < 0)
			{
				std::ostringstream error;

				error << "Invalid argument: last value: " << *last;
				throw service::schema_exception {
					{ service::schema_error { error.str(), {}, _path } }
				};
			}

			if (itrLast - itrFirst > *last)
			{
				itrFirst = itrLast - *last;
			}
		}

		std::vector<std::shared_ptr<_Object>> edges(itrLast - itrFirst);

		std::copy(itrFirst, itrLast, edges.begin());

		return std::make_shared<_Connection>(itrLast<_objects.cend(), itrFirst> _objects.cbegin(),
			std::move(edges));
	}

private:
	const std::shared_ptr<service::RequestState>& _state;
	const service::field_path _path;
	const vec_type& _objects;
};

service::FieldResult<std::shared_ptr<object::AppointmentConnection>> Query::getAppointments(
	service::FieldParams&& params, std::optional<int>&& first,
	std::optional<response::Value>&& after, std::optional<int>&& last,
	std::optional<response::Value>&& before) const
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped,
			service::field_path&& path) {
			loadAppointments(state);

			EdgeConstraints<Appointment, AppointmentConnection> constraints(state,
				std::move(path),
				_appointments);
			auto connection = constraints(firstWrapped, afterWrapped, lastWrapped, beforeWrapped);

			return std::static_pointer_cast<object::AppointmentConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before),
		std::move(params.errorPath));
}

service::FieldResult<std::shared_ptr<object::TaskConnection>> Query::getTasks(
	service::FieldParams&& params, std::optional<int>&& first,
	std::optional<response::Value>&& after, std::optional<int>&& last,
	std::optional<response::Value>&& before) const
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped,
			service::field_path&& path) {
			loadTasks(state);

			EdgeConstraints<Task, TaskConnection> constraints(state, std::move(path), _tasks);
			auto connection = constraints(firstWrapped, afterWrapped, lastWrapped, beforeWrapped);

			return std::static_pointer_cast<object::TaskConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before),
		std::move(params.errorPath));
}

service::FieldResult<std::shared_ptr<object::FolderConnection>> Query::getUnreadCounts(
	service::FieldParams&& params, std::optional<int>&& first,
	std::optional<response::Value>&& after, std::optional<int>&& last,
	std::optional<response::Value>&& before) const
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped,
			service::field_path&& path) {
			loadUnreadCounts(state);

			EdgeConstraints<Folder, FolderConnection> constraints(state,
				std::move(path),
				_unreadCounts);
			auto connection = constraints(firstWrapped, afterWrapped, lastWrapped, beforeWrapped);

			return std::static_pointer_cast<object::FolderConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before),
		std::move(params.errorPath));
}

service::FieldResult<std::vector<std::shared_ptr<object::Appointment>>> Query::getAppointmentsById(
	service::FieldParams&& params, std::vector<response::IdType>&& ids) const
{
	std::promise<std::vector<std::shared_ptr<object::Appointment>>> promise;
	std::vector<std::shared_ptr<object::Appointment>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::static_pointer_cast<object::Appointment>(findAppointment(params, id));
		});
	promise.set_value(std::move(result));

	return promise.get_future();
}

service::FieldResult<std::vector<std::shared_ptr<object::Task>>> Query::getTasksById(
	service::FieldParams&& params, std::vector<response::IdType>&& ids) const
{
	std::promise<std::vector<std::shared_ptr<object::Task>>> promise;
	std::vector<std::shared_ptr<object::Task>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::static_pointer_cast<object::Task>(findTask(params, id));
		});
	promise.set_value(std::move(result));

	return promise.get_future();
}

service::FieldResult<std::vector<std::shared_ptr<object::Folder>>> Query::getUnreadCountsById(
	service::FieldParams&& params, std::vector<response::IdType>&& ids) const
{
	std::promise<std::vector<std::shared_ptr<object::Folder>>> promise;
	std::vector<std::shared_ptr<object::Folder>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::static_pointer_cast<object::Folder>(findUnreadCount(params, id));
		});
	promise.set_value(std::move(result));

	return promise.get_future();
}

service::FieldResult<std::shared_ptr<object::NestedType>> Query::getNested(
	service::FieldParams&& params) const
{
	std::promise<std::shared_ptr<object::NestedType>> promise;

	promise.set_value(std::make_shared<NestedType>(std::move(params), 1));

	return promise.get_future();
}

service::FieldResult<std::vector<std::shared_ptr<object::Expensive>>> Query::getExpensive(
	service::FieldParams&& /*params*/) const
{
	std::vector<std::shared_ptr<object::Expensive>> result(Expensive::count);

	for (auto& entry : result)
	{
		entry = std::make_shared<Expensive>();
	}

	return result;
}

Mutation::Mutation(completeTaskMutation&& mutateCompleteTask)
	: _mutateCompleteTask(std::move(mutateCompleteTask))
{
}

service::FieldResult<std::shared_ptr<object::CompleteTaskPayload>> Mutation::applyCompleteTask(
	service::FieldParams&& params, CompleteTaskInput&& input) const
{
	std::promise<std::shared_ptr<object::CompleteTaskPayload>> promise;

	promise.set_value(_mutateCompleteTask(std::move(input)));

	return promise.get_future();
}

std::optional<response::FloatType> Mutation::_setFloat = std::nullopt;

double Mutation::getFloat() noexcept
{
	return *_setFloat;
}

service::FieldResult<response::FloatType> Mutation::applySetFloat(
	service::FieldParams&& params, response::FloatType&& valueArg) const
{
	_setFloat = std::make_optional(valueArg);
	return valueArg;
}

std::stack<CapturedParams> NestedType::_capturedParams;

NestedType::NestedType(service::FieldParams&& params, int depth)
	: depth(depth)
{
	_capturedParams.push({ response::Value(params.operationDirectives),
		response::Value(params.fragmentDefinitionDirectives),
		response::Value(params.fragmentSpreadDirectives),
		response::Value(params.inlineFragmentDirectives),
		std::move(params.fieldDirectives) });
}

service::FieldResult<response::IntType> NestedType::getDepth(service::FieldParams&& params) const
{
	std::promise<response::IntType> promise;

	promise.set_value(depth);

	return promise.get_future();
}

service::FieldResult<std::shared_ptr<object::NestedType>> NestedType::getNested(
	service::FieldParams&& params) const
{
	std::promise<std::shared_ptr<object::NestedType>> promise;

	promise.set_value(std::make_shared<NestedType>(std::move(params), depth + 1));

	return promise.get_future();
}

std::stack<CapturedParams> NestedType::getCapturedParams()
{
	auto result = std::move(_capturedParams);

	return result;
}

std::mutex Expensive::testMutex {};
std::mutex Expensive::pendingExpensiveMutex {};
std::condition_variable Expensive::pendingExpensiveCondition {};
size_t Expensive::pendingExpensive = 0;

std::atomic<size_t> Expensive::instances = 0;

bool Expensive::Reset() noexcept
{
	std::unique_lock pendingExpensiveLock(pendingExpensiveMutex);

	pendingExpensive = 0;
	pendingExpensiveLock.unlock();

	return instances == 0;
}

Expensive::Expensive()
	: order(++instances)
{
}

Expensive::~Expensive()
{
	--instances;
}

service::FieldResult<response::IntType> Expensive::getOrder(service::FieldParams&& params) const
{
	return std::async(
		params.launch,
		[](bool blockAsync, response::IntType instanceOrder) noexcept {
			if (blockAsync)
			{
				// Block all of the Expensive objects in async mode until the count is reached.
				std::unique_lock pendingExpensiveLock(pendingExpensiveMutex);

				if (++pendingExpensive < count)
				{
					pendingExpensiveCondition.wait(pendingExpensiveLock, []() {
						return pendingExpensive == count;
					});
				}

				// Wake up the next Expensive object.
				pendingExpensiveLock.unlock();
				pendingExpensiveCondition.notify_one();
			}

			return instanceOrder;
		},
		params.launch == std::launch::async,
		static_cast<response::IntType>(order));
}

} /* namespace graphql::today */
