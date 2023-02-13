#include "config/config_utils.hpp"

namespace taraxa {

std::string getConfigErr(std::vector<std::string> path) {
  std::string res = "Error in processing configuration file on param: ";
  for (size_t i = 0; i < path.size(); i++) res += path[i] + ".";
  res += " ";
  return res;
}

Json::Value getConfigData(Json::Value root, const std::vector<std::string> &path, bool optional) {
  for (size_t i = 0; i < path.size(); i++) {
    root = root[path[i]];
    if (root.isNull() && !optional) {
      throw ConfigException(getConfigErr(path) + "Element missing: " + path[i]);
    }
  }
  return root;
}

std::string getConfigDataAsString(const Json::Value &root, const std::vector<std::string> &path, bool optional,
                                  std::string value) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asString();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint32_t getConfigDataAsUInt(const Json::Value &root, const std::vector<std::string> &path, bool optional,
                             uint32_t value) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asUInt();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint64_t getConfigDataAsUInt64(const Json::Value &root, const std::vector<std::string> &path) {
  try {
    return getConfigData(root, path).asUInt64();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

bool getConfigDataAsBoolean(const Json::Value &root, const std::vector<std::string> &path, bool optional, bool value) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asBool();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}
}  // namespace taraxa