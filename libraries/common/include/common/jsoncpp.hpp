#pragma once

#include <json/json.h>

namespace taraxa::util {

Json::Value parse_json(std::string_view const& str);
std::string to_string(Json::Value const& json, bool no_indent = true);

void writeJsonToFile(const std::string& file_name, const Json::Value& json);
Json::Value readJsonFromFile(const std::string& file_name);
Json::Value readJsonFromString(const std::string& str);

}  // namespace taraxa::util
