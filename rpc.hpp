/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-28 16:05:18 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-30 17:38:05
 */
 
#ifndef RPC_HPP
#define RPC_HPP
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <rapidjson/document.h>
#include "full_node.hpp"
#include "util.hpp"

namespace taraxa{

struct RpcConfig{
	RpcConfig (std::string json_file):json_file_name(json_file){
		rapidjson::Document doc = loadJsonFile(json_file);
		port = doc["port"].GetUint();
		address = asio::ip::address::from_string(doc["address"].GetString());
	}
	std::string json_file_name;
	uint16_t port;
	boost::asio::ip::address address;
};


class Rpc{
public:
	Rpc(boost::asio::io_context & io, FullNode & node, RpcConfig &conf):
		io_context_(io), acceptor_(io), node_(node), conf_(conf){}
	virtual ~Rpc() = default;
	void start();
	void waitForAccept();
	void stop();
	boost::asio::io_context & getIoContext(){return io_context_;}
private:
	boost::asio::io_context & io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	FullNode & node_;
	RpcConfig & conf_;
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
	RpcConnection(Rpc & rpc, FullNode & node):
		rpc_(rpc), node_sp_(node.getShared()), socket_(rpc.getIoContext()){
			responded.clear();
		}
	virtual ~RpcConnection() = default;
	virtual void read();
	virtual void write_response(std::string msg);
	boost::asio::ip::tcp::socket & getSocket() { return socket_;}
private:
	Rpc &rpc_;
	std::shared_ptr <FullNode> node_sp_;
	boost::asio::ip::tcp::socket socket_;
	boost::beast::flat_buffer buffer_;
	boost::beast::http::request<boost::beast::http::string_body> request_;
	boost::beast::http::response<boost::beast::http::string_body> response_;
	std::atomic_flag responded;
};

class RpcHandler: public std::enable_shared_from_this<RpcHandler>{

public:
	RpcHandler(Rpc & rpc, FullNode &node, std::string const &body , 
		std::function<void(rapidjson::Document const &doc, std::string const & msg)> const &response_handler):
		rpc_(rpc), node_(node), body_(body), responseHandler_(response_handler){}
	void processRequest();
private:
	Rpc & rpc_;
	FullNode & node_;
	std::string body_;
	//rapidjson::Document doc_;
	std::function<void(rapidjson::Document const & doc, std::string const & msg)> responseHandler_;
};

} // namespace taraxa
#endif
