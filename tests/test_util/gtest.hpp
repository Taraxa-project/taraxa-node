#pragma once

#include <gtest/gtest.h>

#include <filesystem>

#include "common/init.hpp"
#include "common/lazy.hpp"
#include "config/config.hpp"
#include "logger/logging.hpp"

namespace fs = std::filesystem;
using ::taraxa::util::lazy::Lazy;

inline auto const DIR = fs::path(__FILE__).parent_path();
inline auto const DIR_CONF = DIR / "conf";

#define TARAXA_TEST_MAIN(_extension)                                    \
  int main(int argc, char **argv) {                                     \
    taraxa::static_init();                                              \
    std::function<void(int, char **)> extension = _extension;           \
    if (extension) {                                                    \
      extension(argc, argv);                                            \
    }                                                                   \
                                                                        \
    auto logging_config = taraxa::logger::CreateDefaultLoggingConfig(); \
    logging_config.outputs.front().verbosity = spdlog::level::err;      \
    taraxa::logger::Logging::get().Init(logging_config, true);          \
                                                                        \
    ::testing::InitGoogleTest(&argc, argv);                             \
    auto res = RUN_ALL_TESTS();                                         \
    taraxa::logger::Logging::get().Deinit(true);                        \
    return res;                                                         \
  }

struct BaseTest : virtual testing::Test {
  testing::UnitTest *current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const *current_test_info = current_test->current_test_info();

  BaseTest() = default;
  virtual ~BaseTest() = default;

  BaseTest(const BaseTest &) = delete;
  BaseTest(BaseTest &&) = delete;
  BaseTest &operator=(const BaseTest &) = delete;
  BaseTest &operator=(BaseTest &&) = delete;
};

struct WithDataDir : virtual BaseTest {
  std::filesystem::path data_dir = std::filesystem::temp_directory_path() / "taraxa_node_tests" /
                                   current_test_info->test_suite_name() / current_test_info->test_case_name();

  WithDataDir() : BaseTest() {
    std::filesystem::remove_all(data_dir);
    std::filesystem::create_directories(data_dir);
  }
  virtual ~WithDataDir() = default;  // { std::filesystem::remove_all(data_dir); }

  WithDataDir(const WithDataDir &) = delete;
  WithDataDir(WithDataDir &&) = delete;
  WithDataDir &operator=(const WithDataDir &) = delete;
  WithDataDir &operator=(WithDataDir &&) = delete;
};
