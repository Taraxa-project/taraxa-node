#include "rpc.hpp"
#include "full_node.hpp"
#include "wallet.hpp"

namespace taraxa{

RpcConfig::RpcConfig (std::string const &json_file):json_file_name(json_file){
		rapidjson::Document doc = loadJsonFile(json_file);

		assert(doc.HasMember("port"));
		assert(doc.HasMember("address"));

		port = doc["port"].GetUint();
		address = boost::asio::ip::address::from_string(doc["address"].GetString());
	}

Rpc::Rpc(std::string const & conf_rpc, boost::asio::io_context & io, FullNode & node, Wallet & wallet):
		conf_(RpcConfig(conf_rpc)), io_context_(io), acceptor_(io), node_(node), wallet_(wallet){}

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
	std::shared_ptr<RpcConnection> conn_sp (new RpcConnection (*this, node_, wallet_));
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

RpcConnection::RpcConnection(Rpc & rpc, FullNode & node, Wallet & wallet):
	rpc_(rpc), node_sp_(node.getShared()), wallet_sp_(wallet.getShared()), socket_(rpc.getIoContext()){
	responded.clear();
}

void RpcConnection::read(){
	auto this_sp = (shared_from_this());
	boost::beast::http::async_read(socket_, buffer_, request_, 
		[this_sp](boost::system::error_code const &ec, size_t byte_transfered){
		if (!ec){
			std::cout<<"POST read ... "<<std::endl;
			
			// define response handler
			auto replier ([this_sp](std::string const & msg){
				// prepare response content
				std::string body = msg;
				this_sp->write_response(msg);
				// async write
				boost::beast::http::async_write(this_sp->socket_,this_sp->response_, 
					[this_sp](boost::system::error_code const &ec, size_t byte_transfered){});
			});
			// pass response handler
			if (this_sp->request_.method() == boost::beast::http::verb::post){
				std::shared_ptr<RpcHandler> rpc_handler ( 
					new RpcHandler(this_sp->rpc_, *this_sp->node_sp_, 
					*this_sp->wallet_sp_, this_sp->request_.body(), replier));
				try{
					rpc_handler->processRequest();
				} catch (...){
					throw;
				}

			}
		}
		else{
			std::cerr<<"Error! RPC conncetion read fail ... "<<ec.message()<<"\n";
		}
		(void) byte_transfered;
	});
}

void RpcConnection::write_response(std::string const & msg){

	if (!responded.test_and_set()){
		response_.set("Content-Type", "application/json");
		response_.set ("Access-Control-Allow-Origin", "*");
		response_.set ("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
		response_.set ("Connection", "close");
		response_.result (boost::beast::http::status::ok);
		response_.body() = msg;
		response_.prepare_payload ();
	}
	else{
		assert(false && "RPC already responded ...\n");
	}
}

RpcHandler::RpcHandler(Rpc & rpc, FullNode &node, Wallet &wallet, std::string const &body , 
	std::function<void(std::string const & msg)> const &response_handler):
	rpc_(rpc), node_(node), wallet_(wallet), 
	body_(body), in_doc_(taraxa::strToJson(body_)), replier_(response_handler){}

void RpcHandler::processRequest(){
	try{
		
		if (!in_doc_.HasMember("action")){
			throw std::runtime_error("Request does not provide action\n");
		}
		std::string action = in_doc_["action"].GetString();
		std::string res;
		
		if (action == "wallet_account_create"){
			if (!in_doc_.HasMember("sk")){
				throw std::runtime_error("Wallet request (wallet_account_create) does not provide secret key [sk]\n");
			}
			std::string sk = in_doc_["sk"].GetString();
			res = wallet_.accountCreate(sk);
		} else if (action == "wallet_account_query"){
			if (!in_doc_.HasMember("address")){
				throw std::runtime_error("Wallet query (wallet_account_query) does not provide account address [address]" );
			}
			std::string address = in_doc_["address"].GetString();
			res = wallet_.accountQuery(address);
		} 
		else if (action == "user_account_query"){
			if (!in_doc_.HasMember("address")){
				throw std::runtime_error("User account query (account_query) does not provide address\n");
			}
			std::string address = in_doc_["address"].GetString();
			res = node_.accountQuery(address);
			
		} else if (action == "block_create"){
			if (!in_doc_.HasMember("prev_hash")){
				throw std::runtime_error("Request (block_create) does not provide prev_hash\n");
			}
			if (!in_doc_.HasMember("address")){
				throw std::runtime_error("Request (block_create) does not provide address\n");
			}
			if (!in_doc_.HasMember("balance")){
				throw std::runtime_error("Request (block_create) does not provide resulting balance\n");
			}
			blk_hash_t prev_hash = in_doc_["prev_hash"].GetString();
			name_t from_address = in_doc_["from_address"].GetString();
			name_t to_address = in_doc_["to_address"].GetString();
			bal_t balance = in_doc_["balance"].GetUint64();

			res = node_.blockCreate(prev_hash, from_address, to_address, balance);
		} else {
			res = "Unknown action "+ action;
		}
		res+="\n";
		replier_(res);
	}
	catch(std::exception const & err){
		std::cerr<<err.what()<<"\n";
		rapidjson::Document dummy;
		replier_(err.what());
	}
	
}
}
