// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// WARNING! Do not edit this file manually, your changes will be overwritten.

#pragma once

#ifndef CALLRESULTOBJECT_H
#define CALLRESULTOBJECT_H

#include "TaraxaSchema.h"

namespace graphql::taraxa::object {
namespace methods::CallResultHas {

template <class TImpl>
concept getDataWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getData(std::move(params)) } };
};

template <class TImpl>
concept getData = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getData() } };
};

template <class TImpl>
concept getGasUsedWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getGasUsed(std::move(params)) } };
};

template <class TImpl>
concept getGasUsed = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getGasUsed() } };
};

template <class TImpl>
concept getStatusWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getStatus(std::move(params)) } };
};

template <class TImpl>
concept getStatus = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getStatus() } };
};

template <class TImpl>
concept beginSelectionSet = requires (TImpl impl, const service::SelectionSetParams params)
{
	{ impl.beginSelectionSet(params) };
};

template <class TImpl>
concept endSelectionSet = requires (TImpl impl, const service::SelectionSetParams params)
{
	{ impl.endSelectionSet(params) };
};

} // namespace methods::CallResultHas

class [[nodiscard]] CallResult final
	: public service::Object
{
private:
	[[nodiscard]] service::AwaitableResolver resolveData(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveGasUsed(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveStatus(service::ResolverParams&& params) const;

	[[nodiscard]] service::AwaitableResolver resolve_typename(service::ResolverParams&& params) const;

	struct [[nodiscard]] Concept
	{
		virtual ~Concept() = default;

		virtual void beginSelectionSet(const service::SelectionSetParams& params) const = 0;
		virtual void endSelectionSet(const service::SelectionSetParams& params) const = 0;

		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getData(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getGasUsed(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getStatus(service::FieldParams&& params) const = 0;
	};

	template <class T>
	struct [[nodiscard]] Model
		: Concept
	{
		Model(std::shared_ptr<T>&& pimpl) noexcept
			: _pimpl { std::move(pimpl) }
		{
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getData(service::FieldParams&& params) const final
		{
			if constexpr (methods::CallResultHas::getDataWithParams<T>)
			{
				return { _pimpl->getData(std::move(params)) };
			}
			else
			{
				static_assert(methods::CallResultHas::getData<T>, R"msg(CallResult::getData is not implemented)msg");
				return { _pimpl->getData() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getGasUsed(service::FieldParams&& params) const final
		{
			if constexpr (methods::CallResultHas::getGasUsedWithParams<T>)
			{
				return { _pimpl->getGasUsed(std::move(params)) };
			}
			else
			{
				static_assert(methods::CallResultHas::getGasUsed<T>, R"msg(CallResult::getGasUsed is not implemented)msg");
				return { _pimpl->getGasUsed() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getStatus(service::FieldParams&& params) const final
		{
			if constexpr (methods::CallResultHas::getStatusWithParams<T>)
			{
				return { _pimpl->getStatus(std::move(params)) };
			}
			else
			{
				static_assert(methods::CallResultHas::getStatus<T>, R"msg(CallResult::getStatus is not implemented)msg");
				return { _pimpl->getStatus() };
			}
		}

		void beginSelectionSet(const service::SelectionSetParams& params) const final
		{
			if constexpr (methods::CallResultHas::beginSelectionSet<T>)
			{
				_pimpl->beginSelectionSet(params);
			}
		}

		void endSelectionSet(const service::SelectionSetParams& params) const final
		{
			if constexpr (methods::CallResultHas::endSelectionSet<T>)
			{
				_pimpl->endSelectionSet(params);
			}
		}

	private:
		const std::shared_ptr<T> _pimpl;
	};

	CallResult(std::unique_ptr<const Concept>&& pimpl) noexcept;

	[[nodiscard]] service::TypeNames getTypeNames() const noexcept;
	[[nodiscard]] service::ResolverMap getResolvers() const noexcept;

	void beginSelectionSet(const service::SelectionSetParams& params) const final;
	void endSelectionSet(const service::SelectionSetParams& params) const final;

	const std::unique_ptr<const Concept> _pimpl;

public:
	template <class T>
	CallResult(std::shared_ptr<T> pimpl) noexcept
		: CallResult { std::unique_ptr<const Concept> { std::make_unique<Model<T>>(std::move(pimpl)) } }
	{
	}

	[[nodiscard]] static constexpr std::string_view getObjectType() noexcept
	{
		return { R"gql(CallResult)gql" };
	}
};

} // namespace graphql::taraxa::object

#endif // CALLRESULTOBJECT_H
