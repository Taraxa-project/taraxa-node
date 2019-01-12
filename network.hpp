/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-11 16:03:02 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-11 16:38:23
 */
 
#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include "util.hpp"

namespace taraxa{

struct UdpNetworkConfig {
	UdpNetworkConfig (std::string const &json_file);
	std::string json_file_name;
	uint16_t udp_port;
	uint16_t network_io_threads;
	uint16_t network_packet_processing_threads;
};

enum class UdpMessageType: uint8_t{
	invalid = 0,
	transactions = 1,
	block = 2
};

struct UdpData{
	uint8_t *buffer;
	size_t sz;
	end_point_udp_t ep;
};

class UdpMessageHeader{
public:
	UdpMessageHeader ();
	void serialize (stream &) const;
	bool deserialize (stream &);
	std::string getString() const;
private:
	bool valid_ = true;
	uint8_t version_min_;
	uint8_t version_using_;
	uint8_t version_max_;
	UdpMessageType type_;
	uint16_t ext_;
};


class UdpMessageParser{
public:
	UdpMessageParser(UdpData *data);
	UdpMessageHeader getHeader();
private:
	// MTU - IP header - UDP header
	static constexpr size_t max_safe_udp_message_size = 508;
	end_point_udp_t sender_;
	UdpMessageHeader header_;
};

class UdpMessageVisitor{
public:
	virtual void publish();
	virtual ~UdpMessageVisitor();
};

class UdpMessage{
public:
	UdpMessage();
	UdpMessage(UdpMessageHeader const & header);
	virtual ~UdpMessage() = default;
	virtual void serialize (stream &strm) const = 0;
	virtual void visit (UdpMessageVisitor & visitor) const = 0;
	virtual inline std::shared_ptr<std::vector<uint8_t>> to_bytes() const {
		std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
		vectorstream stream (*bytes);
		serialize(stream);
		return bytes;
	}
	UdpMessageHeader getHeader();
protected:
	UdpMessageHeader header_;
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
	// Function will block if there are no free buffers
	// Return nullptr if the container has stopped
	UdpData * allocate ();
	
	// Return a buffer to the freelist after is has been serviced
	void release (UdpData *data);
	
	// Queue a buffer that has been filled with UDP data and notify servicing threads
	void enqueue (UdpData * data);
	
	// Return a buffer that has been filled with UDP data
	// Function will block until a buffer has been added
	// Return nullptr only if the container has stopped and job_qu is empty
	UdpData * dequeue();
	
	// Stop container and notify waiting threads
	// Once the container stopped, cannot allocate,
	// but can consume remaining jobs in job_qu until all jobs are dequeued and relased them back to free_qu
	void stop();
	bool isStopped();
	void print(const std::string &str);
	void setVerbose(bool verbose);
private:
	std::mutex mutex_;
	std::mutex mutex_for_print_;
	std::condition_variable condition_free_qu_;
	std::condition_variable condition_job_qu_;

	boost::circular_buffer<UdpData*> free_qu_;
	boost::circular_buffer<UdpData*> job_qu_;
	std::vector<uint8_t> mem_pool_;
	std::vector<UdpData> entries_;
	bool stopped_;
	bool verbose_;
};

// It has two parts
// 1. Packet receiving, multi-threaded, async io
// 2. Packet processing, multi-threaded

class Network{
public:
	Network (boost::asio::io_context & io_context, std::string const & conf_file_name);
	~Network ();
	void start();
	void stop();
	void receivePackets ();
	void processPackets();
	void parsePacket (UdpData *);
	void rpcAction(boost::system::error_code const & ec, size_t size);
	void sendBuffer(uint8_t const * buffer, size_t sz, end_point_udp_t const & ep, std::function<void(boost::system::error_code const &ec, size_t sz)>);
	void sendTest(end_point_udp_t const & ep);
	UdpNetworkConfig getConfig();
	// for debugging
	void setVerbose(bool verbose);
	void print (std::string const &str);
	unsigned getReceivedPacket();
	unsigned getSentPacket();
private:
	static const size_t BUFFER_SIZE = 512; 
	static const size_t BUFFER_COUNT = 4096;
	UdpNetworkConfig conf_;
	unsigned udp_port_;
	bool on_ = true;
	
	boost::asio::io_context & io_context_;
	resolver_udp_t resolver_;
	socket_udp_t socket_;
	end_point_udp_t ep_ = end_point_udp_t ();
	std::mutex socket_mutex_;
	std::mutex verbose_mutex_;
	UdpBuffer udp_buffer_;
	uint16_t num_io_threads_;
	uint16_t num_packet_processing_threads_;
	std::vector<boost::thread> packet_processing_threads_;

	// for debugging
	bool verbose_ = false;
	unsigned long long num_received_packet_ = 0.0;
	unsigned long long num_sent_packet_ = 0.0;
};



}
#endif
