#pragma once

#include <string>

#include "common/config_exception.hpp"
#include "common/types.hpp"
#include "common/util.hpp"

namespace taraxa {
std::string getConfigErr(std::vector<std::string> path);

Json::Value getConfigData(Json::Value root, const std::vector<std::string> &path, bool optional = false);

std::string getConfigDataAsString(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                                  std::string value = {});

uint32_t getConfigDataAsUInt(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                             uint32_t value = 0);

uint64_t getConfigDataAsUInt64(const Json::Value &root, const std::vector<std::string> &path);

bool getConfigDataAsBoolean(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                            bool value = false);

Json::Value getJsonFromFileOrString(const Json::Value &value);

}  // namespace taraxa