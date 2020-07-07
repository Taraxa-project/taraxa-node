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
#include <vector>

#include "config.hpp"
#include "network.hpp"
#include "top.hpp"
#include "util/lazy.hpp"

namespace taraxa::core_tests::util {
using boost::filesystem::is_regular_file;
using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using boost::filesystem::remove_all;
using dev::KeyPair;
using dev::Secret;
using std::cout;
using std::enable_if_t;
using std::endl;
using std::is_base_of;
using std::vector;
using ::taraxa::util::lazy::Lazy;

inline vector<const char*> const conf_file = {
    "./core_tests/conf/conf_taraxa1.json",
    "./core_tests/conf/conf_taraxa2.json",
    "./core_tests/conf/conf_taraxa3.json",
    "./core_tests/conf/conf_taraxa4.json",
    "./core_tests/conf/conf_taraxa5.json"};

inline vector<vector<const char*>> const conf_input = {
    {"./build/main", "--conf_taraxa", conf_file[0], "--destroy_db"},
    {"./build/main", "--conf_taraxa", conf_file[1], "--destroy_db"},
    {"./build/main", "--conf_taraxa", conf_file[2], "--destroy_db"},
    {"./build/main", "--conf_taraxa", conf_file[3], "--destroy_db"},
    {"./build/main", "--conf_taraxa", conf_file[4],
     "--destroy_db"}};

inline vector<vector<const char*>> const conf_input_persist_db = {
    {"./build/main", "--conf_taraxa", conf_file[0]},
    {"./build/main", "--conf_taraxa", conf_file[1]}};

inline path const config_dir = "./core_tests/conf";
inline auto const all_configs = Lazy([] {
  vector<FullNodeConfig> ret;
  for (auto& entry : recursive_directory_iterator(config_dir)) {
    if (is_regular_file(entry)) {
      ret.emplace_back(entry.path().string());
    }
  }
  return move(ret);
});

inline void cleanAllTestDB() {
  cout << "Removing database directories" << endl;
  for (auto& cfg : *all_configs) {
    remove_all(cfg.db_path);
  }
}

pair<vector<unique_ptr<Top>>, vector<shared_ptr<FullNode>>>
createNodesAndVerifyConnection(int count, int min_connections = 1,
                               bool persist = false, float tests_speed = 1) {
  for (int retries = 0; retries < 3; retries++) {
    pair<vector<unique_ptr<Top>>, vector<shared_ptr<FullNode>>> result;
    vector<unique_ptr<Top>>& tops = result.first;
    vector<shared_ptr<FullNode>>& nodes = result.second;
    assert(count > 0);
    vector<vector<const char*>> conf;
    if (persist) {
      conf = conf_input_persist_db;
    } else {
      conf = conf_input;
    }
    string tests_speed_conf = "--tests_speed";
    string tests_speed_val = to_string(tests_speed);

    if (tests_speed != 1) {
      for (auto& conf_it : conf) {
        conf_it.push_back(tests_speed_conf.c_str());
        conf_it.push_back(tests_speed_val.c_str());
      }
    }
    assert(count <= conf.size());
    for (int i = 0; i < count; i++) {
      taraxa::thisThreadSleepForMilliSeconds(50);
      tops.emplace_back(
          make_unique<Top>(conf[i].size(), (const char**)&conf[i][0]));
      nodes.emplace_back(tops[i]->getNode());
    }
    taraxa::thisThreadSleepForMilliSeconds(200);
    if (count > 1) {
      bool allConnected = true;
      for (int i = 0; i < 100; i++) {
        taraxa::thisThreadSleepForMilliSeconds(200);
        allConnected = true;
        for (int j = 0; j < count; j++) {
          allConnected &= (nodes[j]->getNetwork()->getPeerCount() >= min_connections);
        }
        if (allConnected) break;
      }
      if (allConnected) {
        std::cout << "All connected";
        for (int i = 0; i < count; i++)
          std::cout << " " << nodes[i]->getNetwork()->getPeerCount();
        std::cout << std::endl;
        return result;
      }
    } else
      return result;
  }
  EXPECT_TRUE(false);
  return pair<vector<unique_ptr<Top>>, vector<shared_ptr<FullNode>>>();
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
  string command =
      fmt(R"(top -l 1 -o mem | grep %s | awk '{print $8}')", proc_name);
#else
  // Other environment (need fix)
  string command = fmt(R"(top -n 1 | grep %s | awk '{print $11}')", proc_name);
#endif
  auto res = SysExec(command.c_str());

  return res.substr(0, res.find_first_of('M'));
}

};  // namespace taraxa::core_tests::util

#endif  // TARAXA_NODE_CORE_TESTS_UTIL_HPP_
