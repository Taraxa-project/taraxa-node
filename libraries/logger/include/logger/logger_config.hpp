#pragma once

#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <map>
#include <string>
#include <vector>

#include "common/types.hpp"

namespace taraxa::logger {

// Logger verbosity
// this enum must match enum in aleth logs to corectly support aleths library
enum Verbosity {
  Silent = -1,
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
  Trace = 4,
};

/**
 * @brief Transforms string verbosity to Verbosity enum
 *
 * @param _verbosity
 * @return Verbosity enum
 */
Verbosity stringToVerbosity(std::string _verbosity);

class Config {
 public:
  template <class T>
  using log_sink = boost::log::sinks::synchronous_sink<T>;

  struct OutputConfig {
    OutputConfig() = default;

    std::string type = "console";
    std::string target;
    std::string file_name;
    uint64_t rotation_size = 0;
    std::string time_based_rotation;
    std::string format = "%ThreadID% %ShortNodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%";
    uint64_t max_size = 0;
  };
  Config() = default;
  Config(fs::path log_path);
  ~Config();

  Config(const Config& other);
  Config& operator=(const Config& other);
  Config(Config&& other) noexcept;
  Config& operator=(Config&& other) noexcept;

  /**
   * @brief Init logging - creates boost sinks according to the Config
   */
  void InitLogging(addr_t const& node);

  /**
   * @brief Deinit logging - removes created boost sinks
   */
  void DeinitLogging();

  std::string name = "default";
  Verbosity verbosity{Verbosity::Error};
  std::map<std::string, uint16_t> channels;
  std::vector<OutputConfig> outputs;
  std::vector<boost::shared_ptr<log_sink<boost::log::sinks::text_ostream_backend>>> console_sinks;
  std::vector<boost::shared_ptr<log_sink<boost::log::sinks::text_file_backend>>> file_sinks;

 private:
  bool logging_initialized_{false};
};

}  // namespace taraxa::logger
