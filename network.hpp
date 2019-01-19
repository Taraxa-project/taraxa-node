/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-11 16:03:02 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:13:00
 */
 
#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>
#include <boost/thread.hpp>
#include "udp_buffer.hpp"
#include "util.hpp"
#include "state_block.hpp"

namespace taraxa{

struct UdpData; 
class FullNode;

struct UdpNetworkConfig {
	UdpNetworkConfig (std::string const &json_file);
	std::string json_file_name;
	uint16_t udp_port;
	uint16_t network_io_threads;
	uint16_t network_packet_processing_threads;
	uint32_t udp_buffer_size;
	uint32_t udp_buffer_count;
};

enum class UdpMessageType: uint8_t{
	invalid = 0,
	block = 1,
	trans = 2
};

class UdpMessageHeader{
public:
	UdpMessageHeader ();
	UdpMessageHeader (uint8_t min, uint8_t use, uint8_t max, UdpMessageType type, uint16_t ext);
	void serialize (stream &) const;
	bool deserialize (stream &);
	std::string getString() const;
	UdpMessageType getMessageType() const;
private:
	bool valid_ = true;
	uint8_t version_min_;
	uint8_t version_using_;
	uint8_t version_max_;
	UdpMessageType type_;
	uint16_t ext_;
};

/**
 * Unpack the UDP message, deserialize header, 
 * based on message type, call different deserializer
 */

class UdpParseReceivingMessage{
public:
	UdpParseReceivingMessage(std::shared_ptr<FullNode> full_node);

	void parse(UdpData *data);
private:
	// MTU - IP header - UDP header
	static constexpr size_t max_safe_udp_message_size = 508;
	bool verbose_;
	std::shared_ptr<FullNode> full_node_;
};

/**
 * base class of messages, only contains header data
 * use serializer in subclass
 */

class UdpMessage{
public:
	UdpMessage();
	UdpMessage(UdpMessageHeader const & header);
	virtual ~UdpMessage() = default;
	virtual void serialize (stream &strm) const = 0;
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
 * It has two parts
 * 1. Packet receiving, multi-threaded, async io
 * 2. Packet processing, multi-threaded
 */

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
	void sendBlock(end_point_udp_t const & ep, StateBlock const & blk);
	UdpNetworkConfig getConfig();
	// no need to set full node in network testing
	void setFullNodeAndMsgParser(std::shared_ptr<FullNode> full_node);

	// for debugging
	void setVerbose(bool verbose);
	void setDebug(bool debug);
	void print (std::string const &str);
	unsigned getReceivedPacket();
	unsigned getSentPacket();
private:

	UdpNetworkConfig conf_;
	bool on_ = true;
	
	boost::asio::io_context & io_context_;
	resolver_udp_t resolver_;
	socket_udp_t socket_;
	end_point_udp_t ep_ = end_point_udp_t ();
	std::mutex socket_mutex_;
	std::mutex verbose_mutex_;
	std::shared_ptr<UdpBuffer> udp_buffer_;
	std::shared_ptr<FullNode> full_node_;
	std::shared_ptr<UdpParseReceivingMessage> udp_message_parser_;
	uint16_t num_io_threads_;
	uint16_t num_packet_processing_threads_;
	std::vector<boost::thread> packet_processing_threads_;

	// for debugging
	bool verbose_ = false;
	bool debug_ = false;
	std::mutex debug_mutex_;
	uint64_t num_received_packet_ = 0;
	uint64_t num_sent_packet_ = 0;
};



}
#endif
