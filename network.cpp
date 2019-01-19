#include "network.hpp"
#include "full_node.hpp"
#include "visitor.hpp"

namespace taraxa{

// UdpNetworkCongif ------------------------------

UdpNetworkConfig::UdpNetworkConfig (std::string const &json_file):json_file_name(json_file){
	rapidjson::Document doc = loadJsonFile(json_file);
	
	assert(doc.HasMember("udp_port"));
	assert(doc.HasMember("network_io_threads"));
	assert(doc.HasMember("network_packet_processing_threads"));
	assert(doc.HasMember("udp_buffer_count"));
	assert(doc.HasMember("udp_buffer_size"));

	udp_port = doc["udp_port"].GetUint();
	network_io_threads = doc["network_io_threads"].GetUint();
	network_packet_processing_threads = doc["network_packet_processing_threads"].GetUint();
	udp_buffer_count = doc["udp_buffer_count"].GetUint();
	udp_buffer_size = doc["udp_buffer_size"].GetUint();

}

// MessageHeader ---------------------------------

UdpMessageHeader::UdpMessageHeader(): 
	version_min_(0), version_using_(1), version_max_(1), type_(UdpMessageType::invalid), ext_(0){}

UdpMessageHeader::UdpMessageHeader(uint8_t min, uint8_t use, uint8_t max, UdpMessageType type, uint16_t ext):
	version_min_(min), version_using_(use), version_max_(max), type_(type), ext_(ext){} 

std::string UdpMessageHeader::getString() const {
	std::string s = "Header Version (max, using, min, type, ext) = ("+
		std::to_string(version_max_)+", "+
		std::to_string(version_using_)+", "+
		std::to_string(version_min_)+", "+
		std::to_string(asInteger(type_))+", "+
		std::to_string(EXIT_FAILURE)+")\n";
	return s;
}

UdpMessageType UdpMessageHeader::getMessageType() const { return type_;}

bool UdpMessageHeader::deserialize(stream &strm){
	bool ok = true;
	ok &= read(strm, version_min_);
	ok &= read(strm, version_using_);
	ok &= read(strm, version_max_);
	ok &= read(strm, type_);
	ok &= read(strm, ext_);
	valid_ = ok; 
	assert(ok);
	return ok;
}

void UdpMessageHeader::serialize(stream &strm) const {
	bool ok = true; 
	ok &= write(strm, version_min_);
	ok &= write(strm, version_using_);
	ok &= write(strm, version_max_);
	ok &= write(strm, type_);
	ok &= write(strm, ext_);
	assert(ok);
}

// UdpParseReceivingMessage --------------------------------

UdpParseReceivingMessage::UdpParseReceivingMessage(std::shared_ptr<FullNode> full_node): 
	verbose_(false), full_node_(full_node){}

void UdpParseReceivingMessage::parse(UdpData *data)
{
	end_point_udp_t & sender = data->ep;
	size_t sz = data->sz;
	uint8_t const *buf = data->buffer;
	assert(sz <= UdpParseReceivingMessage::max_safe_udp_message_size);
	bufferstream strm (buf, sz);
	// parse header
	UdpMessageHeader header;
	header.deserialize(strm);
	if (!full_node_){
		std::cout<<"Full node not set in message parser ..."<<std::endl;
		return;
	}
	// based on message type, call different deserializer
	if (header.getMessageType() == UdpMessageType::block){
		// parse block 
		BlockVisitor visitor(full_node_);
		visitor.visit(strm);
	}
} 

// UdpMessage --------------------------------------
UdpMessage::UdpMessage() = default;
UdpMessage::UdpMessage(UdpMessageHeader const & header): header_(header){}
UdpMessageHeader UdpMessage::getHeader(){return header_;}


// Test message ------------------------------------

class UdpMessageTest : public UdpMessage{
public:
	UdpMessageTest() = default;
	UdpMessageTest(UdpMessageHeader const & header):UdpMessage(header){}
	void serialize(stream &strm) const override{
		header_.serialize(strm);
	}
};

// Block message -----------------------------------
class UdpBlockMessage : public UdpMessage{
public: 
	UdpBlockMessage () = default;
	UdpBlockMessage (StateBlock const & block): UdpMessage(UdpMessageHeader(0,1,1, UdpMessageType::block, 0)), block_(block){}
	void serialize(stream &strm) const override{
		header_.serialize(strm);
		block_.serialize(strm);
	}

private:
	StateBlock block_;
};

// Network ----------------------------------------

Network::Network(boost::asio::io_context & io_context , std::string const & conf_file_name) try :  
	conf_(conf_file_name),
	io_context_(io_context),
	resolver_(io_context),
	socket_(io_context, end_point_udp_t (boost::asio::ip::udp::v4(), conf_.udp_port)),
	udp_buffer_(std::make_shared<UdpBuffer> (conf_.udp_buffer_count, conf_.udp_buffer_size)),
	full_node_(nullptr),
	udp_message_parser_(nullptr),
	num_io_threads_(conf_.network_io_threads),
	num_packet_processing_threads_(conf_.network_packet_processing_threads){

	for (auto i = 0; i < num_packet_processing_threads_; ++i){
		packet_processing_threads_.push_back(boost::thread([this](){
			try {
				processPackets();
			}
			catch (...){
				// Do something ...
				std::cerr<<"Cannot create process threads ... \n";
			}
		}));
	}
	
} 
catch (std::exception& e ){
	std::cerr<<"Construct Network Error ... "<<e.what()<<"\n";
	throw e;
} 
Network::~Network(){
	for ( auto & t: packet_processing_threads_) {
		t.join();
	}
}
void Network::setFullNodeAndMsgParser(std::shared_ptr<FullNode> full_node){ 
	full_node_ = full_node;
	udp_message_parser_ = std::make_shared<UdpParseReceivingMessage>(full_node);
}

UdpNetworkConfig Network::getConfig() {return conf_;}
void Network::start(){
	if (verbose_){
		std::cout<<"UDP Network start on port "<<conf_.udp_port<<", io threads = "
		<<num_io_threads_<<", packet processing threads = "<< num_packet_processing_threads_ <<std::endl;
	}
	for (auto i =0; i<num_io_threads_; ++i){
		receivePackets();
	}
}

void Network::stop(){
	on_ = false;
	socket_.close();
	resolver_.cancel();
	udp_buffer_->stop();
}

void Network::setVerbose(bool verbose) { verbose_ = verbose;}
void Network::setDebug(bool debug) { debug_ = debug;}

unsigned Network::getReceivedPacket() {return num_received_packet_;}
unsigned Network::getSentPacket() {return num_sent_packet_;}

void Network::print(std::string const & str){
	std::unique_lock<std::mutex> lock(verbose_mutex_);
	std::cout<<str;
}

void Network::receivePackets(){
	std::unique_lock<std::mutex> lock(socket_mutex_);
	auto data (udp_buffer_->allocate());
	socket_.async_receive_from(boost::asio::buffer(data->buffer, conf_.udp_buffer_size), data->ep, 
		[this, data](boost::system::error_code const & error, size_t sz){
		if (!error && this->on_){
			data->sz = sz;
			this->udp_buffer_->enqueue(data);
			if (debug_){
				std::unique_lock<std::mutex> lock(debug_mutex_);
				num_received_packet_++;
			}
			this->receivePackets();
		} 
		else {
			this->udp_buffer_->release(data);
			if (error == boost::system::errc::operation_canceled){
				//ok due to connection cancelled
				//std::out<<"Operation cancelled ..."<<std::endl;
			}
			else if (error){
				std::cerr<<"Error! Receive packet error ... "<<error.message()<<"\n";
				// error control
			}
		}
	});
}

void Network::processPackets(){
	while (on_){
		auto data (udp_buffer_->dequeue());
		if (data == nullptr){ // implies upd buffer stopped and job queue are empty
			break;
		}
		parsePacket(data);
		udp_buffer_->release(data);
	}
}
void Network::parsePacket(UdpData *data){
	// extract data 
	if (!full_node_){
		if (verbose_){
			std::cout<<"Warning! Fullnode unknown, cannot parse package ...\n";
		}
		return;
	}
	udp_message_parser_->parse(data);
}

void Network::sendBuffer(uint8_t const * buffer, size_t sz, end_point_udp_t const & ep, 
	std::function<void(boost::system::error_code const &ec, size_t sz)> callback){
	// Q: why share the same socket with receive???
	std::unique_lock<std::mutex> lock(socket_mutex_);
	if (debug_){
		std::unique_lock<std::mutex> lock(debug_mutex_);
		num_sent_packet_++;
	}
	socket_.async_send_to(boost::asio::buffer(buffer, sz), ep, [this, callback](boost::system::error_code const & ec, size_t sz){
		callback(ec, sz);
	});
}

void Network::sendTest(end_point_udp_t const &ep){
	UdpMessageTest msg;
	if (verbose_){
		print("Sent ===> "+msg.getHeader().getString());
	}
	auto bytes = msg.to_bytes();
	sendBuffer(bytes->data(), bytes->size(), ep, [this, bytes, ep](boost::system::error_code const &ec, size_t sz){
	});
}

void Network::sendBlock(end_point_udp_t const &ep, StateBlock const & blk){
	UdpBlockMessage msg(blk);
	if (verbose_){
		print("Sent Block, block hash ===> " + blk.getHash().toString());
	}
	auto bytes = msg.to_bytes();
	sendBuffer(bytes->data(), bytes->size(), ep, [this, bytes, ep](boost::system::error_code const &ec, size_t sz){
	});
}

}
