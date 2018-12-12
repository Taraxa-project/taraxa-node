/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-28 16:05:18 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-12 00:44:52
 */
 
#ifndef RPC_HPP
#define RPC_HPP
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <rapidjson/document.h>
#include "util.hpp"

namespace taraxa{

class FullNode;
class Wallet;

struct RpcConfig{
	RpcConfig (std::string const &json_file);
	std::string json_file_name;
	uint16_t port;
	boost::asio::ip::address address;
};


class Rpc{
public:
	Rpc(std::string const & conf_rpc, boost::asio::io_context & io, FullNode & node, Wallet & wallet);
	virtual ~Rpc() = default;
	void start();
	void waitForAccept();
	void stop();
	boost::asio::io_context & getIoContext(){return io_context_;}
private:
	RpcConfig conf_;
	boost::asio::io_context & io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	FullNode & node_;
	Wallet & wallet_;
	
};
// QQ:
// Why RpcConnection and Rpc use different io_context?
// Rpc use a io_context to create acceptor
// RpcConnection use the io_context from node to create socket

// node and rpc use same io_context

// QQ:
// atomic_flag responded, is RpcConnection multithreaded??

class RpcConnection: public std::enable_shared_from_this<RpcConnection> {
public:
	RpcConnection(Rpc & rpc, FullNode & node, Wallet & wallet);
	virtual ~RpcConnection() = default;
	virtual void read();
	virtual void write_response(std::string const & msg);
	boost::asio::ip::tcp::socket & getSocket() { return socket_;}
private:
	Rpc &rpc_;
	std::shared_ptr <FullNode> node_sp_;
	std::shared_ptr <Wallet> wallet_sp_;
	boost::asio::ip::tcp::socket socket_;
	boost::beast::flat_buffer buffer_;
	boost::beast::http::request<boost::beast::http::string_body> request_;
	boost::beast::http::response<boost::beast::http::string_body> response_;
	std::atomic_flag responded;
};

class RpcHandler: public std::enable_shared_from_this<RpcHandler>{

public:
	RpcHandler(Rpc & rpc, FullNode &node, Wallet &wallet, std::string const &body , 
		std::function<void(std::string const & msg)> const &response_handler);
	void processRequest();
private:
	bool accountCreate();
	Rpc & rpc_;
	FullNode & node_;
	Wallet & wallet_;
	std::string body_;
	rapidjson::Document in_doc_;
	std::function<void(std::string const & msg)> replier_; // Capture RpcConnector and reply from there
};

} // namespace taraxa
#endif
