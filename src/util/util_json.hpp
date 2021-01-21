#ifndef TARAXA_NODE__UTIL_JSON_HPP_
#define TARAXA_NODE__UTIL_JSON_HPP_

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

#include "json/reader.h"
#include "json/writer.h"

namespace taraxa::util_json {

using namespace std;
using namespace Json;

inline const auto DEFAULT_READER = CharReaderBuilder().newCharReader();
inline const auto DEFAULT_WRITER = StreamWriterBuilder().newStreamWriter();

inline tuple<Value, bool, JSONCPP_STRING> fromStringNothrow(string_view const& str) {
  Value ret;
  Reader reader;
  bool success = reader.parse(str.data(), str.data() + str.size(), ret);
  return {move(ret), success, "could not parse the json"};
  // TODO
  //  Value ret;
  //  JSONCPP_STRING err;
  //  auto success =
  //      DEFAULT_READER->parse(str.data(), str.data() + str.size(), &ret,
  //      &err);
  //  return {move(ret), success, move(err)};
}

inline Value fromString(string_view const& str) {
  auto [val, success, err] = fromStringNothrow(str);
  if (!success) {
    throw runtime_error(err);
  }
  return val;
}

inline string toString(Value const& json) {
  return json.toStyledString();
  // TODO
  //  stringstream ss;
  //  DEFAULT_WRITER->write(json, &ss);
  //  return ss.str();
}

}  // namespace taraxa::util_json

#endif  // TARAXA_NODE__UTIL_JSON_HPP_
