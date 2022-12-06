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
  virtual ~WithDataDir() { std::filesystem::remove_all(data_dir); }

  WithDataDir(const WithDataDir &) = delete;
  WithDataDir(WithDataDir &&) = delete;
  WithDataDir &operator=(const WithDataDir &) = delete;
  WithDataDir &operator=(WithDataDir &&) = delete;
};
