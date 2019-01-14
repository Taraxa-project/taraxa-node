/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 15:47:31 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-14 12:49:50
 */
 #ifndef TYPES_HPP
 #define TYPES_HPP
 #include <string> 

#include <boost/asio.hpp>


namespace taraxa{

//newtork related
using end_point_udp_t = boost::asio::ip::udp::endpoint;
using socket_udp_t = boost::asio::ip::udp::socket;
using resolver_udp_t = boost::asio::ip::udp::resolver; 

using key_t = std::string;
using name_t = std::string;
using sig_t = std::string;
using blk_hash_t = std::string;

using trx_hash_t = std::string;
using vec_tip_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;
}

#endif
