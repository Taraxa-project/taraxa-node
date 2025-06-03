#pragma once

#include <spdlog/common.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace taraxa {

struct LoggingConfig {
  /**
   * @brief Logging type
   */
  enum class LoggingType { Console, File };

  /**
   * @brief By default all loggers are registered to sinks without specified channels. If sink config has specified
   *        channels, only those channels are logged in such sink
   */
  struct SinkConfig {
    SinkConfig() = default;

    LoggingType type{LoggingType::Console};
    std::string name{"console"};
    std::string file_name;
    std::optional<std::filesystem::path> file_path;
    std::filesystem::path file_full_path;  // file_path(or default value) + file_name
    std::string format{"[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"};
    spdlog::level::level_enum verbosity{spdlog::level::err};
    uint64_t rotation_size{10000000};
    uint64_t max_files_num{20};
    bool on{true};
    std::optional<std::vector<std::string>> channels;
  };

  std::unordered_map<std::string, spdlog::level::level_enum> channels;
  std::vector<SinkConfig> outputs;
};

}  // namespace taraxa
