#include "network.hpp"
#include "full_node.hpp"

namespace taraxa{


// UdpNetworkCongif ------------------------------

UdpNetworkConfig::UdpNetworkConfig (std::string const &json_file):json_file_name(json_file){
	rapidjson::Document doc = loadJsonFile(json_file);
	
	assert(doc.HasMember("udp_port"));
	assert(doc.HasMember("network_io_threads"));
	assert(doc.HasMember("network_packet_processing_threads"));

	udp_port = doc["udp_port"].GetUint();
	network_io_threads = doc["network_io_threads"].GetUint();
	network_packet_processing_threads = doc["network_packet_processing_threads"].GetUint();
}

// MessageHeader ---------------------------------

UdpMessageHeader::UdpMessageHeader(): 
	version_min_(12), version_using_(13), version_max_(14),type_(UdpMessageType::invalid), ext_(1){
}

std::string UdpMessageHeader::getString() const {
	std::string s = "Header Version (max, using, min, type, ext) = ("+
		std::to_string(version_max_)+", "+
		std::to_string(version_using_)+", "+
		std::to_string(version_min_)+", "+
		std::to_string(asInteger(type_))+", "+
		std::to_string(EXIT_FAILURE)+")\n";
	return s;
}

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

// UdpMessageParser --------------------------------
UdpMessageParser::UdpMessageParser(UdpData* data){
	end_point_udp_t & sender = data->ep;
	size_t sz = data->sz;
	uint8_t const *buf = data->buffer;
	assert(sz <= UdpMessageParser::max_safe_udp_message_size);
	bufferstream strm (buf, sz);
	header_.deserialize(strm);
} 
UdpMessageHeader UdpMessageParser::getHeader() {return header_;}

// UdpMessage --------------------------------------
UdpMessage::UdpMessage(){}
UdpMessage::UdpMessage(UdpMessageHeader const & header): header_(header){}
UdpMessageHeader UdpMessage::getHeader(){return header_;}
// Test message ...........

class UdpMessageTest : public UdpMessage{
public:
	UdpMessageTest(){}
	UdpMessageTest(UdpMessageHeader const & header):UdpMessage(header){}
	void serialize(stream &strm) const override{
		header_.serialize(strm);
	}
	void visit(UdpMessageVisitor &visitor) const override {}
};

// UdpBuffer ------------------------------------

UdpBuffer::UdpBuffer(size_t count, size_t sz):
	free_qu_(count), 
	job_qu_(count), 
	mem_pool_(count*sz), 
	entries_(count),
	stopped_(false),
	verbose_(false){
	assert (count>0);
	auto mems_ (mem_pool_.data());
	auto entry_data (entries_.data()); 
	for (size_t i=0; i<count; ++i, ++entry_data){
		*entry_data = {mems_ + i*sz, 0, end_point_udp_t ()};
		free_qu_.push_back(entry_data);
	}
}

UdpData* UdpBuffer::allocate(){
	if (verbose_) print("[lock] UdpBuffer allocate ...");
	std::unique_lock<std::mutex> lock(mutex_);

	while (!stopped_ && free_qu_.empty()){
		if (verbose_) print("[wait] UdpBuffer allocate ...");
		condition_free_qu_.wait(lock);
		if (verbose_) print("[wake up] UdpBuffer allocate ...");
	}

	UdpData *data = nullptr;
	if (!stopped_){
		if (!free_qu_.empty()){
			data = free_qu_.front();
			free_qu_.pop_front();
		}
	}
	return data;
}

void UdpBuffer::release(UdpData *data){
	assert(data != nullptr);
	if (verbose_) print("[lock] UdpBuffer release ...");
	std::unique_lock<std::mutex> lock(mutex_);
	free_qu_.push_back(data);
	condition_free_qu_.notify_one();
	if (verbose_) print("[notify] UdpBuffer release ...");

}

void UdpBuffer::enqueue(UdpData *data){
	assert(data!=nullptr);
	if (verbose_) print("[lock] UdpBuffer enqueue ...");
	std::unique_lock<std::mutex> lock(mutex_);
	job_qu_.push_back(data);
	condition_job_qu_.notify_one();
	if (verbose_) print("[notify] UdpBuffer enqueue ...");

}

UdpData * UdpBuffer::dequeue(){
	
	if (verbose_) print("[lock] UdpBuffer dequeue ...");
	std::unique_lock<std::mutex> lock(mutex_);
	while (!stopped_ && job_qu_.empty()){
		if (verbose_) print("[wait] UdpBuffer dequeue ...");
		condition_job_qu_.wait(lock);
		if (verbose_) print("[wake up] UdpBuffer dequeue ...");
	}
	UdpData *data = nullptr;
	if (!job_qu_.empty()){
		data = job_qu_.front();
		job_qu_.pop_front();
	}
	return data;
}
void UdpBuffer::stop(){
	std::unique_lock<std::mutex> lock(mutex_);
	stopped_ = true;
	condition_job_qu_.notify_all();
	condition_free_qu_.notify_all();

}
bool UdpBuffer::isStopped() {
	std::unique_lock<std::mutex> lock(mutex_);
	return stopped_;
}

void UdpBuffer::print(std::string const & str){
	std::unique_lock<std::mutex> lock(mutex_for_print_);
	std::cout<<str<<std::endl;
}

void UdpBuffer::setVerbose(bool verbose){
	verbose_ = verbose;
}

// Network ----------------------------------------

Network::Network(boost::asio::io_context & io_context , std::string const & conf_file_name) try :  
	conf_(conf_file_name),
	io_context_(io_context),
	resolver_(io_context),
	socket_(io_context, end_point_udp_t (boost::asio::ip::udp::v4(), conf_.udp_port)),
	udp_buffer_(BUFFER_COUNT, BUFFER_SIZE),
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


UdpNetworkConfig Network::getConfig() {return conf_;}
void Network::start(){
	if (verbose_){
		std::cout<<"UDP Network start on port "<<udp_port_<<", io threads = "
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
	udp_buffer_.stop();
}

void Network::setVerbose(bool verbose) { verbose_=verbose;}
unsigned Network::getReceivedPacket() {return num_received_packet_;}
unsigned Network::getSentPacket() {return num_sent_packet_;}

void Network::print(std::string const & str){
	std::unique_lock<std::mutex> lock(verbose_mutex_);
	std::cout<<str;
}

void Network::receivePackets(){
	std::unique_lock<std::mutex> lock(socket_mutex_);
	auto data (udp_buffer_.allocate());
	socket_.async_receive_from(boost::asio::buffer(data->buffer, BUFFER_SIZE), data->ep, 
		[this, data](boost::system::error_code const & error, size_t sz){
		if (!error && this->on_){
			data->sz = sz;
			this->udp_buffer_.enqueue(data);
			num_received_packet_++;
			this->receivePackets();
		} 
		else {
			this->udp_buffer_.release(data);
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
		auto data (udp_buffer_.dequeue());
		if (data == nullptr){ // implies upd buffer stopped and job queue are empty
			break;
		}
		parsePacket(data);
		udp_buffer_.release(data);
	}
}
void Network::parsePacket(UdpData *data){
	// extract data 
	UdpMessageParser parsed (data);
	if (verbose_){
		print("Received -->" + parsed.getHeader().getString());
	}
}

void Network::sendBuffer(uint8_t const * buffer, size_t sz, end_point_udp_t const & ep, 
	std::function<void(boost::system::error_code const &ec, size_t sz)> callback){
	// Q: why share the same socket with receive???
	std::unique_lock<std::mutex> lock(socket_mutex_);
	num_sent_packet_++;
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

}
