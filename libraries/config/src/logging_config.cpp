#include "config/logging_config.hpp"

#include "common/config_exception.hpp"
#include "config/config_utils.hpp"

namespace taraxa {

/**
 * @brief Transforms string verbosity to Verbosity enum
 *
 * @param verbosity
 * @return Verbosity enum
 */
spdlog::level::level_enum stringToVerbosity(std::string verbosity) {
  if (verbosity == "OFF") return spdlog::level::off;
  if (verbosity == "CRITICAL") return spdlog::level::critical;
  if (verbosity == "ERROR") return spdlog::level::err;
  if (verbosity == "WARN") return spdlog::level::warn;
  if (verbosity == "INFO") return spdlog::level::info;
  if (verbosity == "DEBUG") return spdlog::level::debug;
  if (verbosity == "TRACE") return spdlog::level::trace;
  throw ConfigException("Unknown verbosity string: " + verbosity);
}

/**
 * @brief Transforms string logging type to LoggingType enum
 *
 * @param logging_type string
 * @return LoggingType enum
 */
LoggingConfig::LoggingType stringToLoggingType(std::string logging_type) {
  if (logging_type == "console") return LoggingConfig::LoggingType::Console;
  if (logging_type == "file") return LoggingConfig::LoggingType::File;
  throw ConfigException("Unknown logging type string: " + logging_type);
}

void dec_json(const Json::Value &json, LoggingConfig &obj, std::filesystem::path data_path) {
  for (auto &ch : json["channels"]) {
    obj.channels[getConfigDataAsString(ch, {"name"})] = stringToVerbosity(getConfigDataAsString(ch, {"verbosity"}));
  }

  for (auto &o : json["outputs"]) {
    LoggingConfig::SinkConfig output;
    output.name = getConfigDataAsString(o, {"name"});
    output.type = stringToLoggingType(getConfigDataAsString(o, {"type"}));
    output.format = getConfigDataAsString(o, {"format"});
    output.verbosity = stringToVerbosity(getConfigDataAsString(o, {"verbosity"}));
    output.on = getConfigDataAsBoolean(o, {"on"});

    if (auto channels = getConfigData(json, {"channels"}, true); !channels.isNull()) {
      for (auto &ch : channels) {
        output.channels->push_back(ch.asString());
      }
    }

    if (output.type == LoggingConfig::LoggingType::File) {
      if (auto path = getConfigData(json, {"file_path"}, true); !path.isNull()) {
        output.file_path = path.asString();
      } else {
        output.file_path = data_path / "logs";
      }
      output.file_name = getConfigDataAsString(o, {"file_name"});
      output.file_full_path = *output.file_path / output.file_name;
      output.rotation_size = getConfigDataAsUInt(o, {"rotation_size"});
      output.max_files_num = getConfigDataAsUInt(o, {"max_files_num"});
    }

    obj.outputs.push_back(std::move(output));
  }
}

}  // namespace taraxa