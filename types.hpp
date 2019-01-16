/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 15:47:31 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-15 12:52:23
 */

#ifndef TYPES_HPP
#define TYPES_HPP
#include <string> 
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/asio.hpp>


namespace taraxa{
using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

//newtork related
using end_point_udp_t = boost::asio::ip::udp::endpoint;
using socket_udp_t = boost::asio::ip::udp::socket;
using resolver_udp_t = boost::asio::ip::udp::resolver; 


// use uint256_t as intermediate type, 
// uint256_vairant <------> uint256_t <------> stringstream  <------> string
//                  number         encode/decode            operator>>
struct uint256_hash_t{
	uint256_hash_t () = default;
	uint256_hash_t (uint256_t const & number);
	// the string input should be in HEX format
	uint256_hash_t (std::string const & str);
	uint256_hash_t (const char * cstr);
	std::string toString() const ;
	uint256_t number() const;
	void encodeHex(std::string & str) const;
	bool decodeHex(std::string const & str);
	void clear();
	void operator= (std::string const &str);
	bool operator== (uint256_hash_t const & other) const;

	// debugging
	void rawPrint() const;

	std::array<uint8_t,32>		bytes;
};
std::ostream & operator<<(std::ostream & strm, uint256_hash_t const &num);

using key_t = std::string;
using name_t = std::string;
using sig_t = std::string;
using blk_hash_t = std::string;
using trx_hash_t = std::string;
using vec_tip_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;





}

#endif
