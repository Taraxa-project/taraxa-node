#include "http_processor.hpp"

namespace taraxa::net {

HttpProcessor::Response HttpProcessor::createOkResponse(std::string&& response_body) {
  Response response;
  response.set("Content-Type", "application/json");
  response.set("Access-Control-Allow-Origin", "*");
  response.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
  response.set("Connection", "close");
  response.result(boost::beast::http::status::ok);
  response.body() = std::move(response_body);
  response.prepare_payload();

  return response;
}

HttpProcessor::Response HttpProcessor::createErrResponse(std::string&& response_body) {
  Response response;
  response.set("Content-Type", "application/json");
  response.set("Access-Control-Allow-Origin", "*");
  response.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
  response.set("Connection", "close");
  response.result(boost::beast::http::status::bad_request);
  response.body() = std::move(response_body);
  response.prepare_payload();

  return response;
}

}  // namespace taraxa::net