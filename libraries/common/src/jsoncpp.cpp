#include "common/jsoncpp.hpp"

#include "common/util.hpp"

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

void writeJsonToFile(const std::string& file_name, const Json::Value& json) {
  std::ofstream ofile(file_name, std::ios::trunc);

  if (ofile.is_open()) {
    ofile << json;
  } else {
    std::stringstream err;
    err << "Cannot open file " << file_name << std::endl;
    throw std::invalid_argument(err.str());
  }
}

Json::Value readJsonFromFile(const std::string& file_name) {
  std::ifstream ifile(file_name);
  if (ifile.is_open()) {
    Json::Value json;
    ifile >> json;
    ifile.close();
    return json;
  } else {
    throw std::invalid_argument(std::string("Cannot open file ") + file_name);
  }
}

Json::Value readJsonFromString(const std::string& str) {
  Json::CharReaderBuilder builder;
  Json::CharReader* reader = builder.newCharReader();
  Json::Value json;
  std::string errors;

  bool parsingSuccessful = reader->parse(str.c_str(), str.c_str() + str.size(), &json, &errors);
  delete reader;

  if (!parsingSuccessful) {
    throw std::invalid_argument(std::string("Cannot parse json"));
  }

  return json;
}

}  // namespace taraxa::util
