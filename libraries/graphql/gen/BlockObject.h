// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// WARNING! Do not edit this file manually, your changes will be overwritten.

#pragma once

#ifndef BLOCKOBJECT_H
#define BLOCKOBJECT_H

#include "TaraxaSchema.h"

namespace graphql::taraxa::object {
namespace methods::BlockHas {

template <class TImpl>
concept getNumberWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getNumber(std::move(params)) } };
};

template <class TImpl>
concept getNumber = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getNumber() } };
};

template <class TImpl>
concept getHashWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getHash(std::move(params)) } };
};

template <class TImpl>
concept getHash = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getHash() } };
};

template <class TImpl>
concept getParentWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableObject<std::shared_ptr<Block>> { impl.getParent(std::move(params)) } };
};

template <class TImpl>
concept getParent = requires (TImpl impl)
{
	{ service::AwaitableObject<std::shared_ptr<Block>> { impl.getParent() } };
};

template <class TImpl>
concept getNonceWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getNonce(std::move(params)) } };
};

template <class TImpl>
concept getNonce = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getNonce() } };
};

template <class TImpl>
concept getTransactionsRootWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getTransactionsRoot(std::move(params)) } };
};

template <class TImpl>
concept getTransactionsRoot = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getTransactionsRoot() } };
};

template <class TImpl>
concept getTransactionCountWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<std::optional<int>> { impl.getTransactionCount(std::move(params)) } };
};

template <class TImpl>
concept getTransactionCount = requires (TImpl impl)
{
	{ service::AwaitableScalar<std::optional<int>> { impl.getTransactionCount() } };
};

template <class TImpl>
concept getStateRootWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getStateRoot(std::move(params)) } };
};

template <class TImpl>
concept getStateRoot = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getStateRoot() } };
};

template <class TImpl>
concept getReceiptsRootWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getReceiptsRoot(std::move(params)) } };
};

template <class TImpl>
concept getReceiptsRoot = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getReceiptsRoot() } };
};

template <class TImpl>
concept getMinerWithParams = requires (TImpl impl, service::FieldParams params, std::optional<response::Value> blockArg)
{
	{ service::AwaitableObject<std::shared_ptr<Account>> { impl.getMiner(std::move(params), std::move(blockArg)) } };
};

template <class TImpl>
concept getMiner = requires (TImpl impl, std::optional<response::Value> blockArg)
{
	{ service::AwaitableObject<std::shared_ptr<Account>> { impl.getMiner(std::move(blockArg)) } };
};

template <class TImpl>
concept getExtraDataWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getExtraData(std::move(params)) } };
};

template <class TImpl>
concept getExtraData = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getExtraData() } };
};

template <class TImpl>
concept getGasLimitWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getGasLimit(std::move(params)) } };
};

template <class TImpl>
concept getGasLimit = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getGasLimit() } };
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
concept getTimestampWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getTimestamp(std::move(params)) } };
};

template <class TImpl>
concept getTimestamp = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getTimestamp() } };
};

template <class TImpl>
concept getLogsBloomWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getLogsBloom(std::move(params)) } };
};

template <class TImpl>
concept getLogsBloom = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getLogsBloom() } };
};

template <class TImpl>
concept getMixHashWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getMixHash(std::move(params)) } };
};

template <class TImpl>
concept getMixHash = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getMixHash() } };
};

template <class TImpl>
concept getDifficultyWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getDifficulty(std::move(params)) } };
};

template <class TImpl>
concept getDifficulty = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getDifficulty() } };
};

template <class TImpl>
concept getTotalDifficultyWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getTotalDifficulty(std::move(params)) } };
};

template <class TImpl>
concept getTotalDifficulty = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getTotalDifficulty() } };
};

template <class TImpl>
concept getOmmerCountWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<std::optional<int>> { impl.getOmmerCount(std::move(params)) } };
};

template <class TImpl>
concept getOmmerCount = requires (TImpl impl)
{
	{ service::AwaitableScalar<std::optional<int>> { impl.getOmmerCount() } };
};

template <class TImpl>
concept getOmmersWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Block>>>> { impl.getOmmers(std::move(params)) } };
};

template <class TImpl>
concept getOmmers = requires (TImpl impl)
{
	{ service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Block>>>> { impl.getOmmers() } };
};

template <class TImpl>
concept getOmmerAtWithParams = requires (TImpl impl, service::FieldParams params, int indexArg)
{
	{ service::AwaitableObject<std::shared_ptr<Block>> { impl.getOmmerAt(std::move(params), std::move(indexArg)) } };
};

template <class TImpl>
concept getOmmerAt = requires (TImpl impl, int indexArg)
{
	{ service::AwaitableObject<std::shared_ptr<Block>> { impl.getOmmerAt(std::move(indexArg)) } };
};

template <class TImpl>
concept getOmmerHashWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableScalar<response::Value> { impl.getOmmerHash(std::move(params)) } };
};

template <class TImpl>
concept getOmmerHash = requires (TImpl impl)
{
	{ service::AwaitableScalar<response::Value> { impl.getOmmerHash() } };
};

template <class TImpl>
concept getTransactionsWithParams = requires (TImpl impl, service::FieldParams params)
{
	{ service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Transaction>>>> { impl.getTransactions(std::move(params)) } };
};

template <class TImpl>
concept getTransactions = requires (TImpl impl)
{
	{ service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Transaction>>>> { impl.getTransactions() } };
};

template <class TImpl>
concept getTransactionAtWithParams = requires (TImpl impl, service::FieldParams params, int indexArg)
{
	{ service::AwaitableObject<std::shared_ptr<Transaction>> { impl.getTransactionAt(std::move(params), std::move(indexArg)) } };
};

template <class TImpl>
concept getTransactionAt = requires (TImpl impl, int indexArg)
{
	{ service::AwaitableObject<std::shared_ptr<Transaction>> { impl.getTransactionAt(std::move(indexArg)) } };
};

template <class TImpl>
concept getLogsWithParams = requires (TImpl impl, service::FieldParams params, BlockFilterCriteria filterArg)
{
	{ service::AwaitableObject<std::vector<std::shared_ptr<Log>>> { impl.getLogs(std::move(params), std::move(filterArg)) } };
};

template <class TImpl>
concept getLogs = requires (TImpl impl, BlockFilterCriteria filterArg)
{
	{ service::AwaitableObject<std::vector<std::shared_ptr<Log>>> { impl.getLogs(std::move(filterArg)) } };
};

template <class TImpl>
concept getAccountWithParams = requires (TImpl impl, service::FieldParams params, response::Value addressArg)
{
	{ service::AwaitableObject<std::shared_ptr<Account>> { impl.getAccount(std::move(params), std::move(addressArg)) } };
};

template <class TImpl>
concept getAccount = requires (TImpl impl, response::Value addressArg)
{
	{ service::AwaitableObject<std::shared_ptr<Account>> { impl.getAccount(std::move(addressArg)) } };
};

template <class TImpl>
concept getCallWithParams = requires (TImpl impl, service::FieldParams params, CallData dataArg)
{
	{ service::AwaitableObject<std::shared_ptr<CallResult>> { impl.getCall(std::move(params), std::move(dataArg)) } };
};

template <class TImpl>
concept getCall = requires (TImpl impl, CallData dataArg)
{
	{ service::AwaitableObject<std::shared_ptr<CallResult>> { impl.getCall(std::move(dataArg)) } };
};

template <class TImpl>
concept getEstimateGasWithParams = requires (TImpl impl, service::FieldParams params, CallData dataArg)
{
	{ service::AwaitableScalar<response::Value> { impl.getEstimateGas(std::move(params), std::move(dataArg)) } };
};

template <class TImpl>
concept getEstimateGas = requires (TImpl impl, CallData dataArg)
{
	{ service::AwaitableScalar<response::Value> { impl.getEstimateGas(std::move(dataArg)) } };
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

} // namespace methods::BlockHas

class [[nodiscard]] Block final
	: public service::Object
{
private:
	[[nodiscard]] service::AwaitableResolver resolveNumber(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveHash(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveParent(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveNonce(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTransactionsRoot(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTransactionCount(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveStateRoot(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveReceiptsRoot(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveMiner(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveExtraData(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveGasLimit(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveGasUsed(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTimestamp(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveLogsBloom(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveMixHash(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveDifficulty(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTotalDifficulty(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveOmmerCount(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveOmmers(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveOmmerAt(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveOmmerHash(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTransactions(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveTransactionAt(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveLogs(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveAccount(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveCall(service::ResolverParams&& params) const;
	[[nodiscard]] service::AwaitableResolver resolveEstimateGas(service::ResolverParams&& params) const;

	[[nodiscard]] service::AwaitableResolver resolve_typename(service::ResolverParams&& params) const;

	struct [[nodiscard]] Concept
	{
		virtual ~Concept() = default;

		virtual void beginSelectionSet(const service::SelectionSetParams& params) const = 0;
		virtual void endSelectionSet(const service::SelectionSetParams& params) const = 0;

		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getNumber(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getHash(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<Block>> getParent(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getNonce(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getTransactionsRoot(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<std::optional<int>> getTransactionCount(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getStateRoot(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getReceiptsRoot(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<Account>> getMiner(service::FieldParams&& params, std::optional<response::Value>&& blockArg) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getExtraData(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getGasLimit(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getGasUsed(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getTimestamp(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getLogsBloom(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getMixHash(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getDifficulty(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getTotalDifficulty(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<std::optional<int>> getOmmerCount(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Block>>>> getOmmers(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<Block>> getOmmerAt(service::FieldParams&& params, int&& indexArg) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getOmmerHash(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Transaction>>>> getTransactions(service::FieldParams&& params) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<Transaction>> getTransactionAt(service::FieldParams&& params, int&& indexArg) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::vector<std::shared_ptr<Log>>> getLogs(service::FieldParams&& params, BlockFilterCriteria&& filterArg) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<Account>> getAccount(service::FieldParams&& params, response::Value&& addressArg) const = 0;
		[[nodiscard]] virtual service::AwaitableObject<std::shared_ptr<CallResult>> getCall(service::FieldParams&& params, CallData&& dataArg) const = 0;
		[[nodiscard]] virtual service::AwaitableScalar<response::Value> getEstimateGas(service::FieldParams&& params, CallData&& dataArg) const = 0;
	};

	template <class T>
	struct [[nodiscard]] Model
		: Concept
	{
		Model(std::shared_ptr<T>&& pimpl) noexcept
			: _pimpl { std::move(pimpl) }
		{
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getNumber(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getNumberWithParams<T>)
			{
				return { _pimpl->getNumber(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getNumber<T>, R"msg(Block::getNumber is not implemented)msg");
				return { _pimpl->getNumber() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getHash(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getHashWithParams<T>)
			{
				return { _pimpl->getHash(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getHash<T>, R"msg(Block::getHash is not implemented)msg");
				return { _pimpl->getHash() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<Block>> getParent(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getParentWithParams<T>)
			{
				return { _pimpl->getParent(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getParent<T>, R"msg(Block::getParent is not implemented)msg");
				return { _pimpl->getParent() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getNonce(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getNonceWithParams<T>)
			{
				return { _pimpl->getNonce(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getNonce<T>, R"msg(Block::getNonce is not implemented)msg");
				return { _pimpl->getNonce() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getTransactionsRoot(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getTransactionsRootWithParams<T>)
			{
				return { _pimpl->getTransactionsRoot(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTransactionsRoot<T>, R"msg(Block::getTransactionsRoot is not implemented)msg");
				return { _pimpl->getTransactionsRoot() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<std::optional<int>> getTransactionCount(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getTransactionCountWithParams<T>)
			{
				return { _pimpl->getTransactionCount(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTransactionCount<T>, R"msg(Block::getTransactionCount is not implemented)msg");
				return { _pimpl->getTransactionCount() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getStateRoot(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getStateRootWithParams<T>)
			{
				return { _pimpl->getStateRoot(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getStateRoot<T>, R"msg(Block::getStateRoot is not implemented)msg");
				return { _pimpl->getStateRoot() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getReceiptsRoot(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getReceiptsRootWithParams<T>)
			{
				return { _pimpl->getReceiptsRoot(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getReceiptsRoot<T>, R"msg(Block::getReceiptsRoot is not implemented)msg");
				return { _pimpl->getReceiptsRoot() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<Account>> getMiner(service::FieldParams&& params, std::optional<response::Value>&& blockArg) const final
		{
			if constexpr (methods::BlockHas::getMinerWithParams<T>)
			{
				return { _pimpl->getMiner(std::move(params), std::move(blockArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getMiner<T>, R"msg(Block::getMiner is not implemented)msg");
				return { _pimpl->getMiner(std::move(blockArg)) };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getExtraData(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getExtraDataWithParams<T>)
			{
				return { _pimpl->getExtraData(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getExtraData<T>, R"msg(Block::getExtraData is not implemented)msg");
				return { _pimpl->getExtraData() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getGasLimit(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getGasLimitWithParams<T>)
			{
				return { _pimpl->getGasLimit(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getGasLimit<T>, R"msg(Block::getGasLimit is not implemented)msg");
				return { _pimpl->getGasLimit() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getGasUsed(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getGasUsedWithParams<T>)
			{
				return { _pimpl->getGasUsed(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getGasUsed<T>, R"msg(Block::getGasUsed is not implemented)msg");
				return { _pimpl->getGasUsed() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getTimestamp(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getTimestampWithParams<T>)
			{
				return { _pimpl->getTimestamp(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTimestamp<T>, R"msg(Block::getTimestamp is not implemented)msg");
				return { _pimpl->getTimestamp() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getLogsBloom(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getLogsBloomWithParams<T>)
			{
				return { _pimpl->getLogsBloom(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getLogsBloom<T>, R"msg(Block::getLogsBloom is not implemented)msg");
				return { _pimpl->getLogsBloom() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getMixHash(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getMixHashWithParams<T>)
			{
				return { _pimpl->getMixHash(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getMixHash<T>, R"msg(Block::getMixHash is not implemented)msg");
				return { _pimpl->getMixHash() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getDifficulty(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getDifficultyWithParams<T>)
			{
				return { _pimpl->getDifficulty(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getDifficulty<T>, R"msg(Block::getDifficulty is not implemented)msg");
				return { _pimpl->getDifficulty() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getTotalDifficulty(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getTotalDifficultyWithParams<T>)
			{
				return { _pimpl->getTotalDifficulty(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTotalDifficulty<T>, R"msg(Block::getTotalDifficulty is not implemented)msg");
				return { _pimpl->getTotalDifficulty() };
			}
		}

		[[nodiscard]] service::AwaitableScalar<std::optional<int>> getOmmerCount(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getOmmerCountWithParams<T>)
			{
				return { _pimpl->getOmmerCount(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getOmmerCount<T>, R"msg(Block::getOmmerCount is not implemented)msg");
				return { _pimpl->getOmmerCount() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Block>>>> getOmmers(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getOmmersWithParams<T>)
			{
				return { _pimpl->getOmmers(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getOmmers<T>, R"msg(Block::getOmmers is not implemented)msg");
				return { _pimpl->getOmmers() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<Block>> getOmmerAt(service::FieldParams&& params, int&& indexArg) const final
		{
			if constexpr (methods::BlockHas::getOmmerAtWithParams<T>)
			{
				return { _pimpl->getOmmerAt(std::move(params), std::move(indexArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getOmmerAt<T>, R"msg(Block::getOmmerAt is not implemented)msg");
				return { _pimpl->getOmmerAt(std::move(indexArg)) };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getOmmerHash(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getOmmerHashWithParams<T>)
			{
				return { _pimpl->getOmmerHash(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getOmmerHash<T>, R"msg(Block::getOmmerHash is not implemented)msg");
				return { _pimpl->getOmmerHash() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::optional<std::vector<std::shared_ptr<Transaction>>>> getTransactions(service::FieldParams&& params) const final
		{
			if constexpr (methods::BlockHas::getTransactionsWithParams<T>)
			{
				return { _pimpl->getTransactions(std::move(params)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTransactions<T>, R"msg(Block::getTransactions is not implemented)msg");
				return { _pimpl->getTransactions() };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<Transaction>> getTransactionAt(service::FieldParams&& params, int&& indexArg) const final
		{
			if constexpr (methods::BlockHas::getTransactionAtWithParams<T>)
			{
				return { _pimpl->getTransactionAt(std::move(params), std::move(indexArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getTransactionAt<T>, R"msg(Block::getTransactionAt is not implemented)msg");
				return { _pimpl->getTransactionAt(std::move(indexArg)) };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::vector<std::shared_ptr<Log>>> getLogs(service::FieldParams&& params, BlockFilterCriteria&& filterArg) const final
		{
			if constexpr (methods::BlockHas::getLogsWithParams<T>)
			{
				return { _pimpl->getLogs(std::move(params), std::move(filterArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getLogs<T>, R"msg(Block::getLogs is not implemented)msg");
				return { _pimpl->getLogs(std::move(filterArg)) };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<Account>> getAccount(service::FieldParams&& params, response::Value&& addressArg) const final
		{
			if constexpr (methods::BlockHas::getAccountWithParams<T>)
			{
				return { _pimpl->getAccount(std::move(params), std::move(addressArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getAccount<T>, R"msg(Block::getAccount is not implemented)msg");
				return { _pimpl->getAccount(std::move(addressArg)) };
			}
		}

		[[nodiscard]] service::AwaitableObject<std::shared_ptr<CallResult>> getCall(service::FieldParams&& params, CallData&& dataArg) const final
		{
			if constexpr (methods::BlockHas::getCallWithParams<T>)
			{
				return { _pimpl->getCall(std::move(params), std::move(dataArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getCall<T>, R"msg(Block::getCall is not implemented)msg");
				return { _pimpl->getCall(std::move(dataArg)) };
			}
		}

		[[nodiscard]] service::AwaitableScalar<response::Value> getEstimateGas(service::FieldParams&& params, CallData&& dataArg) const final
		{
			if constexpr (methods::BlockHas::getEstimateGasWithParams<T>)
			{
				return { _pimpl->getEstimateGas(std::move(params), std::move(dataArg)) };
			}
			else
			{
				static_assert(methods::BlockHas::getEstimateGas<T>, R"msg(Block::getEstimateGas is not implemented)msg");
				return { _pimpl->getEstimateGas(std::move(dataArg)) };
			}
		}

		void beginSelectionSet(const service::SelectionSetParams& params) const final
		{
			if constexpr (methods::BlockHas::beginSelectionSet<T>)
			{
				_pimpl->beginSelectionSet(params);
			}
		}

		void endSelectionSet(const service::SelectionSetParams& params) const final
		{
			if constexpr (methods::BlockHas::endSelectionSet<T>)
			{
				_pimpl->endSelectionSet(params);
			}
		}

	private:
		const std::shared_ptr<T> _pimpl;
	};

	Block(std::unique_ptr<const Concept>&& pimpl) noexcept;

	[[nodiscard]] service::TypeNames getTypeNames() const noexcept;
	[[nodiscard]] service::ResolverMap getResolvers() const noexcept;

	void beginSelectionSet(const service::SelectionSetParams& params) const final;
	void endSelectionSet(const service::SelectionSetParams& params) const final;

	const std::unique_ptr<const Concept> _pimpl;

public:
	template <class T>
	Block(std::shared_ptr<T> pimpl) noexcept
		: Block { std::unique_ptr<const Concept> { std::make_unique<Model<T>>(std::move(pimpl)) } }
	{
	}

	[[nodiscard]] static constexpr std::string_view getObjectType() noexcept
	{
		return { R"gql(Block)gql" };
	}
};

} // namespace graphql::taraxa::object

#endif // BLOCKOBJECT_H
