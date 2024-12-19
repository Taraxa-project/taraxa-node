#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Json {
class Value;
}  // namespace Json

namespace taraxa {
std::string getConfigErr(const std::vector<std::string> &path);

Json::Value getConfigData(Json::Value root, const std::vector<std::string> &path, bool optional = false);

std::string getConfigDataAsString(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                                  const std::string &value = {});

uint64_t getConfigDataAsUInt(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                             uint32_t value = 0);

bool getConfigDataAsBoolean(const Json::Value &root, const std::vector<std::string> &path, bool optional = false,
                            bool value = false);

Json::Value getJsonFromFileOrString(const Json::Value &value);

}  // namespace taraxa