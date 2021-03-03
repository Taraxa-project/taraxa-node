#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>

namespace taraxa::net {

class HttpProcessor {
 public:
  using Request = boost::beast::http::request<boost::beast::http::string_body>;
  using Response = boost::beast::http::response<boost::beast::http::string_body>;

 public:
  virtual Response process(const Request& request) = 0;

 protected:
  virtual Response createOkResponse(std::string&& response_body = "");
  virtual Response createErrResponse(std::string&& response_body = "");
};

}  // namespace taraxa::net
