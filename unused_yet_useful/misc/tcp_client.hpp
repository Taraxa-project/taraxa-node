/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:38:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 17:37:13
 */
#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <memory>

namespace asio = boost::asio;
using std::string;

class TcpClient{
public: 
	TcpClient():
		context_(){}
		// socket_(std::make_shared<asio::ip::tcp::socket> (context_)){}
	void connect(const string &hostname, unsigned short port);
	void sendMessage(string msg);
	std::string recvMessage();
	void echo(const string &msg); 
	void close();
	void setVerbose(bool verbose) { verbose_ = verbose; } 
private:
	asio::io_context context_;
	std::shared_ptr<asio::ip::tcp::socket> socket_;
	bool connected_ = false;
	bool verbose_ = false;
};
#endif
