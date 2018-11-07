/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 11:18:53 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 17:21:02
 */
 
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <rapidjson/document.h>
#include "tcp_server.hpp"
#include "rocks_db.hpp"

namespace asio = boost::asio;
using std::string;
using std::shared_ptr;

TcpServer::TcpServer(){
	string localhost = asio::ip::host_name();
	asio::ip::tcp::resolver resolver(context_);
	asio::ip::tcp::resolver::query query(localhost);
	std::cout<<"Host Address: "<<localhost<<std::endl;
}
void TcpServer::writeJsonToDb(const string &json){
	rapidjson::Document document;
	document.Parse(json.c_str());
	string hash=document["hash"].GetString();
	db_->put(hash, json);
	if (verbose_){
		print("Write to DB: "+hash+"\n");
	}
}
void TcpServer::listen(const string &hostname, unsigned port, bool echo){
	asio::ip::tcp::endpoint ep(asio::ip::address::from_string(hostname), port);
	asio::ip::tcp::acceptor acceptor(context_, ep);
	listening_=true;
	int i=0;
	while (true){
		if (listening_==false) return;
		shared_ptr<asio::ip::tcp::socket> socket(new asio::ip::tcp::socket(context_));
		acceptor.accept(*socket);
		boost::thread([socket, echo, this](){
			string thread_id="["+boost::lexical_cast<string>(boost::this_thread::get_id())+"]";
			string remote_addr=socket->remote_endpoint().address().to_string();
			string remote_port=std::to_string(socket->remote_endpoint().port());
			print(thread_id+ "Service request from " + remote_addr + ":" + remote_port +"\n");
			boost::system::error_code ec;
			asio::streambuf recvbuf; 
			string data;
			while (true) {
				asio::read_until(*socket, recvbuf, taraxa::EOB, ec);
				if (ec == asio::error::eof){
					print("End of connection ...\n");
					break;
				} else if (ec){
					print("Error reading from client: "+ec.message()+"\n");
					break;
				}
				data = asio::buffer_cast<const char*>(recvbuf.data());
				data.erase(data.size()-taraxa::EOB.size());
				if (data==(taraxa::EOC)){
					print("End of connection received ... abort ...\n");
					break;
				} 

				if (verbose_){
					print(thread_id + "Server Receive -> "+data+"\n");
				}
				
				//TODO: take DB part out
				if (db_){
					writeJsonToDb(data);
				}
				std::size_t s=recvbuf.size();
				recvbuf.consume(s);
				
				if (echo){
					data+=taraxa::EOB;
					asio::write(*socket, asio::buffer(data.c_str(), data.size()));
					if (verbose_){
						print(thread_id + "Server Send -> "+data+"\n");
					}
				}
			}
			print(thread_id + "is done ...");
		}
		);
		std::cout<<"i = "<<i++<<std::endl;
	}
}

// int main(){
// 	TcpServer server;
// 	server.setVerbose(true);
// 	server.setDb("/tmp/myRocksDB");
// 	server.listen("192.168.12.204", 3000, true);
// }