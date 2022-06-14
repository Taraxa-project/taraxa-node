#pragma once

#include <gtest/gtest.h>

#include <filesystem>

#include "common/jsoncpp.hpp"
#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "config/config.hpp"
#include "test_config.hpp"

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
  // data_dir full path name
  // "/var/folders/v9/z_fyy_1923jf7qp1t10mjn440000gn/T/taraxa_node_tests/PbftManagerTest/PbftManagerTest"

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
  const std::vector<std::string> secret_keys{"3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                             "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                             "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f",
                                             "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029",
                                             "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe"};
  const std::vector<std::string> vrf_secret_keys{
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c1df1edc9f3367fba550b7971fc2de6c5998d8784"
      "051c5be69abc9644",
      "90f59a7ee7a392c811c5d299b557a4e09e610de7d109d6b3fcb19ab8d51c9a0d931f5e7db07c9969e438db7e287eabbaaca49ca414f5f3a4"
      "02ea6997ade40081",
      "dfa4e556e1107f1333c4aa4a71eb7faf745b227d30cad99640971ea6680e348e97485c51e033260894132aa326bb1c984a70ac7f4202315b"
      "90e669fb701a8f64",
      "1c15656b71a93e7987e6a164d25d3a645871bcd8b70f30d4454f857c770c2345783a02cd21f22a9f305ccedaef677f16b0d6c48dd9844cb6"
      "76d5178ed628eb92",
      "938003392792a797193703fbcfee25d65e85c6c9f69f25b8324485409c90f85446b69174b3ec82cc136509367c93f1bb616ed16cbc189dc0"
      "afa628c919469c59"};

  const auto nodes_count = 5;
  const auto data_path = "/tmp/taraxa";
  const auto tcp_port = 10002;
  const auto http_port = 7777;
  const auto ws_port = 8776;
  Json::Value conf_json = taraxa::util::readJsonFromString(taraxa::core_tests::test_json);
  Json::Value wallet_json;
  for (auto i = 1; i <= nodes_count; i++) {
    const auto node_data_path = data_path + std::to_string(i);
    conf_json["data_path"] = node_data_path;
    conf_json["network_tcp_port"] = Json::UInt64(tcp_port + i);
    conf_json["rpc"]["http_port"] = Json::UInt64(http_port + i);
    conf_json["rpc"]["ws_port"] = Json::UInt64(ws_port + i);
    conf_json["logging"]["configurations"][0]["outputs"][1]["file_name"] =
        "Taraxa_N" + std::to_string(i) + "_%m%d%Y_%H%M%S_%5N.log";

    std::filesystem::create_directories(node_data_path);
    auto conf_file = node_data_path + (std::string("/conf_taraxa") + std::to_string(i) + ".json");
    taraxa::util::writeJsonToFile(conf_file, conf_json);

    wallet_json["node_secret"] = secret_keys[i - 1];
    wallet_json["vrf_secret"] = vrf_secret_keys[i - 1];
    auto wallet_file = node_data_path + (std::string("/wallet") + std::to_string(i) + ".json");
    taraxa::util::writeJsonToFile(conf_file, conf_json);
    ret.emplace_back(conf_json, wallet_json, conf_file);
  }

  return ret;
});

struct BaseTest : virtual WithDataDir {
  BaseTest() : WithDataDir() { CleanupDirs(); }
  virtual ~BaseTest() {}

  BaseTest(const BaseTest &) = delete;
  BaseTest(BaseTest &&) = delete;
  BaseTest &operator=(const BaseTest &) = delete;
  BaseTest &operator=(BaseTest &&) = delete;

  void CleanupDirs() {
    for (auto &cfg : *node_cfgs_original) {
      remove_all(cfg.data_path);
    }
  }

  void TearDown() override { CleanupDirs(); }
};
