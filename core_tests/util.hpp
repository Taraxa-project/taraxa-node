#ifndef TARAXA_NODE_CORE_TESTS_UTIL_HPP
#define TARAXA_NODE_CORE_TESTS_UTIL_HPP

#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <boost/filesystem.hpp>
#include <string>
#include <type_traits>
#include "config.hpp"
#include "genesis_state.hpp"

namespace taraxa::core_tests::util {
using namespace std;
using namespace dev;
using namespace boost::filesystem;

inline path const config_dir = "./core_tests/conf";
inline auto const all_configs = []() {
  vector<FullNodeConfig> ret;
  for (auto& entry : recursive_directory_iterator(config_dir)) {
    if (is_regular_file(entry)) {
      ret.emplace_back(entry.path().string());
    }
  }
  return move(ret);
}();

inline void cleanAllTestDB() {
  cout << "Removing database directories" << endl;
  for (auto& cfg : all_configs) {
    remove_all(cfg.db_path);
  }
}

template <typename T = ::testing::Test,
          typename = enable_if_t<is_base_of<::testing::Test, T>::value>>
struct DBUsingTest : public T {
  void SetUp() override {
    T::SetUp();
    cleanAllTestDB();
  }

  void TearDown() override {
    T::TearDown();
    // we might need DB for debugging
    // cleanAllTestDB();
  }
};

inline auto addr(Secret const& secret = Secret::random()) {
  return KeyPair(secret).address();
}

inline auto addr(string const& secret_str) { return addr(Secret(secret_str)); }

};  // namespace taraxa::core_tests::util

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_HPP_
