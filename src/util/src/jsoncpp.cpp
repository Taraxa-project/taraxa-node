#include "util/jsoncpp.hpp"

#include "util/util.hpp"

namespace taraxa::util {

Json::Value parse_json(std::string_view const& str) {
  // TODO optimize jsoncpp objects creation
  Json::CharReaderBuilder reader_builder;
  Json::Value ret;
  std::string parse_err;
  if (std::unique_ptr<Json::CharReader>(reader_builder.newCharReader())
          ->parse(str.data(), str.data() + str.size(), &ret, &parse_err)) {
    return ret;
  }
  throw Json::Exception(parse_err);
}

std::string to_string(Json::Value const& json, bool no_indent) {
  // TODO optimize jsoncpp objects creation
  Json::StreamWriterBuilder writer_builder;
  if (no_indent) {
    writer_builder["indentation"] = "";
  }
  return Json::writeString(writer_builder, json);
}

}  // namespace taraxa::util
