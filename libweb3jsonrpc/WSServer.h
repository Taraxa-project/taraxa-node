#include <algorithm>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "libdevcore/Log.h"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

namespace taraxa {

class WSSession : public std::enable_shared_from_this<WSSession> {
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;

 public:
  // Take ownership of the socket
  explicit WSSession(tcp::socket&& socket) : ws_(std::move(socket)) {}

  // Start the asynchronous operation
  void run();

  void on_accept(beast::error_code ec);
  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_write(beast::error_code ec, std::size_t bytes_transferred);
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WSListener : public std::enable_shared_from_this<WSListener> {
  public:
  WSListener(net::io_context& ioc, tcp::endpoint endpoint);

  // Start accepting incoming connections
  void run();

 private:
  void do_accept();
  void on_accept(beast::error_code ec, tcp::socket socket);
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
};
}  // namespace taraxa
//------------------------------------------------------------------------------

/* void startWs(std::string address, unsigned short port, net::io_context)
{
    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // Create and launch a listening port
    std::make_shared<WSListener>(ioc,
tcp::endpoint{net::ip::make_address(address), port})->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();
}*/
