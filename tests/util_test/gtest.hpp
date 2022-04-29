#pragma once

#include <gtest/gtest.h>

#include <filesystem>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "config/config.hpp"

namespace fs = std::filesystem;
using ::taraxa::util::lazy::Lazy;

inline auto const DIR = fs::path(__FILE__).parent_path();
inline auto const DIR_CONF = DIR / "conf";

#define TARAXA_TEST_MAIN(_extension)                          \
  int main(int argc, char **argv) {                           \
    taraxa::static_init();                                    \
    std::function<void(int, char **)> extension = _extension; \
    if (extension) {                                          \
      extension(argc, argv);                                  \
    }                                                         \
    ::testing::InitGoogleTest(&argc, argv);                   \
    return RUN_ALL_TESTS();                                   \
  }

struct WithTestInfo : virtual testing::Test {
  testing::UnitTest *current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const *current_test_info = current_test->current_test_info();

  WithTestInfo() = default;
  virtual ~WithTestInfo() = default;

  WithTestInfo(const WithTestInfo &) = delete;
  WithTestInfo(WithTestInfo &&) = delete;
  WithTestInfo &operator=(const WithTestInfo &) = delete;
  WithTestInfo &operator=(WithTestInfo &&) = delete;
};

struct WithDataDir : virtual WithTestInfo {
  std::filesystem::path data_dir = std::filesystem::temp_directory_path() / "taraxa_node_tests" /
                                   current_test_info->test_suite_name() / current_test_info->test_case_name();

  WithDataDir() : WithTestInfo() {
    std::filesystem::remove_all(data_dir);
    std::filesystem::create_directories(data_dir);
  }
  virtual ~WithDataDir() { std::filesystem::remove_all(data_dir); }

  WithDataDir(const WithDataDir &) = delete;
  WithDataDir(WithDataDir &&) = delete;
  WithDataDir &operator=(const WithDataDir &) = delete;
  WithDataDir &operator=(WithDataDir &&) = delete;
};

inline auto const node_cfgs_original = Lazy([] {
  std::vector<taraxa::FullNodeConfig> ret;
  for (int i = 1;; ++i) {
    auto p = DIR_CONF / (std::string("conf_taraxa") + std::to_string(i) + ".json");
    if (!fs::exists(p)) {
      break;
    }
    auto w = DIR_CONF / (std::string("wallet") + std::to_string(i) + ".json");
    Json::Value test_node_wallet_json;
    std::ifstream(w.string(), std::ifstream::binary) >> test_node_wallet_json;
    ret.emplace_back(p.string(), test_node_wallet_json);
  }
  return ret;
});

struct BaseTest : virtual WithDataDir {
  BaseTest() : WithDataDir() {
    for (auto &cfg : *node_cfgs_original) {
      remove_all(cfg.data_path);
    }
  }
  virtual ~BaseTest() {}

  BaseTest(const BaseTest &) = delete;
  BaseTest(BaseTest &&) = delete;
  BaseTest &operator=(const BaseTest &) = delete;
  BaseTest &operator=(BaseTest &&) = delete;

  void TearDown() override {
    for (auto &cfg : *node_cfgs_original) {
      remove_all(cfg.data_path);
    }
  }
};
