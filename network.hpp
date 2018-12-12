/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-11 16:03:02 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-12 14:01:00
 */
 
#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>
#include <boost/circular_buffer.hpp>
#include "util.hpp"

namespace taraxa{

class FullNode;

struct UdpData{
	uint8_t *buffer;
	size_t sz;
	end_point_udp_t ep;
};

/**
  * A circular buffer for servicing UDP datagrams. 
  * This container follows a producer/consumer model where the operating system is producing data in to buffers which are serviced by internal threads.
  * If buffers are not serviced fast enough they're internally dropped.
  * This container has a maximum space to hold N (count) buffers of M (sz) size and will allocate them in round-robin order.
  * All public methods are thread-safe
*/

class UdpBuffer{
public: 
	UdpBuffer(size_t count, size_t sz);
	
	// Return a buffer where UDP data can be put
	// Method will attempt to return the first free buffer
	// If there are no free buffers, an unserviced buffer (full) will be dequeued and returned
	// Function will block if there are no free or unserviced buffers
	// Return nullptr if the container has stopped
	UdpData * allocate ();
	
	// Return a buffer to the freelist after is has been serviced
	void release (UdpData *data);
	
	// Queue a buffer that has been filled with UDP data and notify servicing threads
	void enqueue (UdpData * data);
	
	// Return a buffer that has been filled with UDP data
	// Function will block until a buffer has been added
	// Return nullptr if the container has stopped
	UdpData * dequeue();
	
	// Stop container and notify waiting threads
	void stop();
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	boost::circular_buffer<UdpData*> free_qu_;
	boost::circular_buffer<UdpData*> job_qu_;
	std::vector<uint8_t> mem_pool_;
	std::vector<UdpData> entries_;
	bool stopped_ = true;
};
class Network{
public:
	Network (FullNode &node, uint16_t port);
	void receive ();
	void processPackets();
	void start();
	void stop();
	void receiveAction (UdpData *);
	void rpcAction(boost::system::error_code const & ec, size_t size);
	void sendBuffer(uint8_t const * buffer, size_t sz, end_point_udp_t const & ep, std::function<void(boost::system::error_code const &ec, size_t sz)>);

private:
	static const size_t BUFFER_SIZE = 512; 
	static const size_t BUFFER_COUNT = 4096;
	static const uint16_t UDP_PORT = 7777;
	FullNode & node_;
	resolver_udp_t resolver_;
	socket_udp_t socket_;
	end_point_udp_t ep_ = end_point_udp_t ();
	std::mutex mutex_;
	UdpBuffer udp_buffer_;
	std::vector<std::thread> processing_threads_;
	
	bool on = true;


};



}
#endif
