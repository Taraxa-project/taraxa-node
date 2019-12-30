#ifndef TARAXA_NODE_CORE_TESTS_UTIL_CONSTANTS_HPP_
#define TARAXA_NODE_CORE_TESTS_UTIL_CONSTANTS_HPP_

#include "types.hpp"

namespace taraxa::core_tests::util::constants {

inline const val_t TEST_MAX_TRANSACTIONS_IN_BLOCK = 10000;
// TODO calculate dynamicall based on the eth chain params
inline const val_t TEST_TX_GAS_LIMIT =
    MOCK_BLOCK_GAS_LIMIT / TEST_MAX_TRANSACTIONS_IN_BLOCK;

}  // namespace taraxa::core_tests::util::constants

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_CONSTANTS_HPP_
