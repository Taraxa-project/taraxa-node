/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 11:18:46 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 17:38:32
 */
 
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <memory> // shared_ptr
#include "tcp_server.hpp"
#include "tcp_client.hpp"

namespace asio = boost::asio;
using std::string;

void TcpClient::connect(const string &hostname, unsigned short port){
	if (connected_) {
		std::cout<<"Socket is alreay connected ... "<<std::endl;
		return;
	}
	asio::ip::tcp::resolver resolver(context_);
	asio::ip::tcp::resolver::query query(hostname, std::to_string(port));
	asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
	asio::ip::tcp::endpoint ep = iter->endpoint();
	std::cout<<"Connecting to "<<ep<<std::endl;
	boost::system::error_code ec;
	socket_.reset(new boost::asio::ip::tcp::socket(context_));
	socket_->connect(ep, ec);
	if (ec){
		std::cerr<<"Error creation connection ... "<<ec.message()<<std::endl;
		connected_ = false;
		return;
	}
	connected_ = true;
	std::cout<<"Connected ... "<<std::endl;
	
}
void TcpClient::sendMessage(string msg){
	if (!connected_) {
		std::cerr<<"Socket is not connected, cannot send message ..."<<std::endl;
		return;
	}
	// Must end with EOB
	msg+=taraxa::EOB;
	boost::system::error_code ec;
	asio::const_buffer buff(msg.c_str(), msg.size());
	asio::write(*socket_, buff, ec);

	if (ec){
		std::cerr<<"Error reading from sever: "<<ec.message()<<std::endl;
		return;
	}
	
	if (verbose_){
		std::cout<<"Client Sent ->"<<msg<<std::endl;
	}
}

std::string TcpClient::recvMessage(){
	if (!connected_){
		std::cerr<<"Socket is not connected, cannot receive message ..."<<std::endl;
		return "";	
	}
	asio::streambuf recvbuf;
	boost::system::error_code ec;
	asio::read_until(*socket_, recvbuf, taraxa::EOB);
	string data = asio::buffer_cast<const char*>(recvbuf.data());
	if (ec){
		std::cerr<<"Error reading from sever: "<<ec.message()<<std::endl;
		return "";
	}
	if (verbose_){
		std::cout<<"Client Receive -> "<<data<<std::endl;	
	}
	return data.erase(data.size()-(taraxa::EOB.size()));
}
void TcpClient::close(){
	socket_->shutdown(asio::ip::tcp::socket::shutdown_send);
	connected_=false;
	if (verbose_){
		std::cout<<"Client close channel ..."<<std::endl;
	}
}

void TcpClient::echo(const string &msg){
	if (msg==taraxa::EOC){
		close();
	}
	sendMessage(msg);
	recvMessage();
}