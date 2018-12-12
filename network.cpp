#include "network.hpp"
#include "full_node.hpp"

namespace taraxa{

UdpBuffer::UdpBuffer(size_t count, size_t sz):
	free_qu_(count), 
	job_qu_(count), 
	mem_pool_(count*sz), 
	entries_(count){
	assert (count>0);
	assert (count>0);
	auto mems_ (mem_pool_.data());
	auto entry_data (entries_.data()); 
	for (size_t i=0; i<count; ++i, ++entry_data){
		*entry_data = {mems_ + i*sz, 0, end_point_udp_t ()};
		free_qu_.push_back(entry_data);
	}
}

UdpData* UdpBuffer::allocate(){
	std::unique_lock<std::mutex> lock(mutex_);

	while (!stopped_ && free_qu_.empty() && job_qu_.empty()){
		condition_.wait(lock);
	}
	UdpData *data = nullptr;
	if (!free_qu_.empty()){
		data = free_qu_.front();
		free_qu_.pop_front();
	} /*else {
		buf = job_qu_.front();
		job_qu_.pop_front();
	}
	*/
	// QQ??
	// free_qu_ can contains freeptr, means its stopping. 
	// return 
	if (data == nullptr){
		data = job_qu_.front();
		job_qu_.pop_front();
	}
	return data;
}

void UdpBuffer::release(UdpData *data){
	assert(data != nullptr);
	std::unique_lock<std::mutex> lock(mutex_);
	free_qu_.push_back(data);
	condition_.notify_one();
}

void UdpBuffer::enqueue(UdpData *data){
	assert(data!=nullptr);
	std::unique_lock<std::mutex> lock(mutex_);
	job_qu_.push_back(data);
	condition_.notify_one();
}

UdpData * UdpBuffer::dequeue(){
	std::unique_lock<std::mutex> lock(mutex_);
	while (!stopped_ && job_qu_.empty()){
		condition_.wait(lock);
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
	condition_.notify_all();
}

Network::Network(FullNode &node, uint16_t UDP_PORT): 
	node_(node),
	resolver_(node.getIoContext()),
	socket_(node.getIoContext(), end_point_udp_t (boost::asio::ip::address_v6::any(), UDP_PORT)),
	udp_buffer_(BUFFER_COUNT, BUFFER_SIZE)
	{

	} 



}
