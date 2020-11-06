#ifndef TARAXA_NODE_UTIL_GTEST_HPP_
#define TARAXA_NODE_UTIL_GTEST_HPP_

#include <gtest/gtest.h>

#include <filesystem>

#include "../static_init.hpp"

#define TARAXA_TEST_MAIN(_extension)                         \
  int main(int argc, char** argv) {                          \
    taraxa::static_init();                                   \
    std::function<void(int, char**)> extension = _extension; \
    if (extension) {                                         \
      extension(argc, argv);                                 \
    }                                                        \
    ::testing::InitGoogleTest(&argc, argv);                  \
    return RUN_ALL_TESTS();                                  \
  }

struct WithTestInfo : virtual testing::Test {
  testing::UnitTest* current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const* current_test_info =
      current_test->current_test_info();

  virtual ~WithTestInfo() {}
};

struct WithDataDir : virtual WithTestInfo {
  std::filesystem::path data_dir = std::filesystem::temp_directory_path() /
                                     "taraxa_node_tests" /
                                     current_test_info->test_suite_name() /
                                     current_test_info->test_case_name();

  WithDataDir() : WithTestInfo() {
    std::filesystem::remove_all(data_dir);
    std::filesystem::create_directories(data_dir);
  }
  virtual ~WithDataDir() { std::filesystem::remove_all(data_dir); }
};

#endif  // TARAXA_NODE_UTIL_GTEST_HPP_
