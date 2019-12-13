#ifndef TARAXA_NODE_CORE_TESTS_UTIL_HPP
#define TARAXA_NODE_CORE_TESTS_UTIL_HPP

#include <gtest/gtest.h>
#include <libdevcrypto/Common.h>
#include <array>
#include <boost/filesystem.hpp>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
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
    cleanAllTestDB();
    T::SetUp();
  }
};

inline auto addr(Secret const& secret = Secret::random()) {
  return KeyPair(secret).address();
}

inline auto addr(string const& secret_str) { return addr(Secret(secret_str)); }

std::string SysExec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}
std::string getMemUsage(string const& proc_name) {
#if defined(__APPLE__)
  string command = fmt(
      R"(top -l 1 -o mem | grep %s | awk '{print $8}')", proc_name);
#else
  string command = fmt(
      R"(top -n 1 | grep %s | awk '{print $11}')", proc_name);
#endif
  auto res = SysExec(command.c_str());

  return res.substr(0, res.find_first_of('M'));
}

};  // namespace taraxa::core_tests::util

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_HPP_
