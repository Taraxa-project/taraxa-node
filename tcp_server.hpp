/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 14:57:36 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 17:59:54
 */
#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include "rocks_db.hpp"

namespace asio = boost::asio;
using std::string;
using std::shared_ptr;

namespace taraxa{ 
	const std::string EOC("X_EOC_X"); // end of connection
	const std::string EOB("X_EOB_X"); // end of block
}

class TcpServer{
public:
	TcpServer();
	void listen(const string &hostname, unsigned port, bool echo=false);
	void setVerbose(bool verbose){verbose_ = verbose;}
	void setDb(std::string path){ db_ = std::make_shared<RocksDb>(path);}
	void close(){ listening_ = false;}
private:
	void writeJsonToDb(const std::string &json);
	void print(string s){
		std::unique_lock<std::mutex> lock(mu_);
		std::cout<<s<<std::endl;
	}
	asio::io_context context_;
	string hostname;
	std::shared_ptr<RocksDb> db_ = nullptr;
	std::mutex mu_;
	bool listening_ = false;
	bool verbose_ = false;
};

class ServerActionHandler{
	
};
#endif
