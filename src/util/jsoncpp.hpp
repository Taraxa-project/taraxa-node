#pragma once

#include <json/json.h>

namespace taraxa::util {

Json::Value parse_json(std::string_view const& str);
std::string to_string(Json::Value const& json, bool no_indent = true);

}  // namespace taraxa::util
