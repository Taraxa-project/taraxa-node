/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 15:47:31 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 13:56:19
 */

#ifndef TYPES_HPP
#define TYPES_HPP
#include <string> 
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/asio.hpp>
#include <type_traits>

namespace taraxa{
using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

//newtork related
using end_point_udp_t = boost::asio::ip::udp::endpoint;
using socket_udp_t = boost::asio::ip::udp::socket;
using resolver_udp_t = boost::asio::ip::udp::resolver; 

/**
 * for example, use uint256_t as intermediate type, 
 * uint_hash_t <------> uint256_t <------> stringstream  <------> string
 *                      number         encode/decode            operator>>
 */

template <std::size_t Bytes>
struct uint_hash_t {
	static_assert ( (Bytes == 16 
		|| Bytes == 32 
		|| Bytes == 64), 
		"Bytes must be 16, 32 or 64\n");

	using Number = typename std::conditional<Bytes == 16, uint128_t, typename std::conditional<Bytes == 32, uint256_t, uint512_t>::type >::type;
	uint_hash_t () = default;
	uint_hash_t (Number const & number);
	uint_hash_t (std::string const & str);
	uint_hash_t (const char * cstr);
	void encodeHex(std::string & str) const;
	bool decodeHex(std::string const & str);

	Number number () const;

	void clear();
	void operator= (std::string const &str);
	bool operator== (uint_hash_t const & other) const;

	std::string toString() const;
	// debugging
	void rawPrint() const;
	std::array<uint8_t, Bytes> bytes;
};

template <std::size_t Byte> 
std::ostream & operator<<(std::ostream & strm, uint_hash_t<Byte> const &num);

using uint256_hash_t = uint_hash_t<32>;
using uint512_hash_t = uint_hash_t<64>;

using key_t = std::string;
using name_t = std::string;
using sig_t = uint512_hash_t;
using blk_hash_t = uint256_hash_t;
using trx_hash_t = uint256_hash_t;
using vec_tip_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;

// std::ostream & operator<<(std::ostream & strm, uint256_hash_t const &num){
// 	strm << std::hex<<num.toString() <<" ";
// 	return strm;
// }

}

#endif
