
#include "jsonrpc_http_processor.hpp"

#include "common/jsoncpp.hpp"
#include "common/util.hpp"
namespace taraxa::net {

HttpProcessor::Response JsonRpcHttpProcessor::process(const Request &request) {
  std::optional<JsonRpcHttpProcessor::Error> err;
  if (request.method() == boost::beast::http::verb::options) {
    response_.set("Allow", "OPTIONS, POST");
    response_.result(boost::beast::http::status::no_content);
  } else if (request.method() == boost::beast::http::verb::post) {
    auto handler = GetHandler();
    assert(handler);
    response_.set("Content-Type", "application/json");
    response_.result(boost::beast::http::status::ok);
    try {
      handler->HandleRequest(request.body(), response_.body());
    } catch (std::exception const &e) {
      err.emplace();
      err->message << "Uncaught API exception: " << e.what();
    }
  }
  if (err) {
    auto const &err_msg = err->message.str();
    Json::Value res_json(Json::objectValue);
    res_json["jsonrpc"] = "2.0";
    auto req_json = util::parse_json(request.body());
    if (req_json.isObject() && req_json.isMember("id") &&  // this conditional was taken from jsonrpccpp sources
        (req_json["id"].isNull() || req_json["id"].isIntegral() || req_json["id"].isString())) {
      res_json["id"] = req_json["id"];
    } else {
      res_json["id"] = Json::nullValue;
    }
    auto &res_json_error = res_json["error"] = Json::Value(Json::objectValue);
    res_json_error["code"] = err->code;
    res_json_error["message"] = err_msg;
    if (!err->data.empty()) {
      res_json_error["data"] = err->data;
    }
    response_.body() = util::to_string(res_json);
  } else {
    response_.result(boost::beast::http::status::method_not_allowed);
  }
  response_.set("Access-Control-Allow-Origin", "*");
  response_.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
  response_.set("Connection", "close");
  response_.prepare_payload();

  return response_;
}

}  // namespace taraxa::net