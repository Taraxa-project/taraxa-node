#include "rpc.hpp"
#include <rapidjson/document.h>

#include <iostream>
#include <sstream>

namespace taraxa{

void Rpc::start(){
	boost::asio::ip::tcp::endpoint ep(conf_.address, conf_.port);
	acceptor_.open(ep.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address (true));

	boost::system::error_code ec;
	acceptor_.bind(ep, ec);
	if (ec){
		std::cerr<<"Error! RPC cannot bind ... "<<ec.message()<<"\n";
		throw std::runtime_error(ec.message());
	}
	acceptor_.listen();
	std::cout<<"RPC is listening on port "<< conf_.port<<std::endl;

	waitForAccept();
}

void Rpc::waitForAccept(){
	std::shared_ptr<RpcConnection> conn_sp (new RpcConnection (*this, node_));
	std::cout<<"Listening ...."<<std::endl;
	acceptor_.async_accept(conn_sp->getSocket(), [this, conn_sp](boost::system::error_code const & ec){
		if (!ec){
			std::cout<<"A connection is accepted"<<std::endl;
			conn_sp->read();
		}
		else {
			std::cerr<<"Error! RPC async_accept error ... "<<ec.message()<<"\n";
			throw std::runtime_error(ec.message());
		}
		waitForAccept();
	});
}

void Rpc::stop(){
	acceptor_.close();
}

void RpcConnection::read(){
	auto this_sp = (shared_from_this());
	boost::beast::http::async_read(socket_, buffer_, request_, 
		[this_sp](boost::system::error_code const &ec, size_t byte_transfered){
		if (!ec){
			std::cout<<"POST read ... "<<std::endl;
			
			// define response handler
			auto responseHandler ([this_sp](rapidjson::Document const & doc, std::string const & msg){
				// prepare response content
				std::stringstream ostr;
				rapidjson::StringBuffer buffer;
				rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
				doc.Accept(writer);
				std::string body = msg+"\n"+ buffer.GetString();
				this_sp->write_response(body);
				// async write
				boost::beast::http::async_write(this_sp->socket_,this_sp->response_, 
					[this_sp](boost::system::error_code const &ec, size_t byte_transfered){});
			});
			// pass response handler
			if (this_sp->request_.method() == boost::beast::http::verb::post){
				std::shared_ptr<RpcHandler> rpc_handler ( new RpcHandler(this_sp->rpc_, *this_sp->node_sp_, this_sp->request_.body(), responseHandler));
				rpc_handler->processRequest();
			}
		}
		else{
			std::cerr<<"Error! RPC conncetion read fail ... "<<ec.message()<<"\n";
		}
	});
}

void RpcConnection::write_response(std::string body){

	if (!responded.test_and_set()){
		response_.set("Content-Type", "application/json");
		response_.set ("Access-Control-Allow-Origin", "*");
		response_.set ("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
		response_.set ("Connection", "close");
		response_.result (boost::beast::http::status::ok);
		response_.body() = body;
		response_.prepare_payload ();
	}
	else{
		assert(false && "RPC already responded ...\n");
	}
}

void RpcHandler::processRequest(){
	try{
		rapidjson::Document in_doc = strToJson(body_);
		std::string action = in_doc["action"].GetString();
	
		// rapidjson::Document outDoc;
		// outDoc.SetObject();
		// auto& allocator = outDoc.GetAllocator();
		// outDoc.AddMember("ok", true, allocator);
		responseHandler_(in_doc, "Echo ...");
	}
	catch(std::exception const & err){
		std::cerr<<err.what()<<"\n";
		rapidjson::Document dummy;
		responseHandler_(dummy, err.what());
	}
	
}
}
