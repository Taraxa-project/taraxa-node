/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:26:50 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-11 16:24:15
 */
 
 #ifndef UTIL_HPP
 #define UTIL_HPP

#include <iostream>
#include <fstream>
#include <streambuf>
#include <string>
#include <boost/asio.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include "types.hpp"
namespace taraxa{

rapidjson::Document strToJson( std::string const & str);
// load file and convert to json doc
rapidjson::Document loadJsonFile( std::string const& json_file_name); 


struct ProcessReturn{
	 enum class Result {
		PROGRESS, 
		BAD_SIG,
		SEEN, 
		NEG_SPEND,
		UNRECEIVABLE, // ?
		MISS_PREV,
		MISS_SOURCE
	 };
	 taraxa::name_t user_account;
};

template <typename enumT> 
auto asInteger(enumT const value){
	return static_cast<typename std::underlying_type<enumT>::type> (value);
}

using stream = std::basic_streambuf<uint8_t>;
using bufferstream = boost::iostreams::stream_buffer<boost::iostreams::basic_array_source<uint8_t>>;
using vectorstream = boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<std::vector<uint8_t>>>;

// Read a raw byte stream the size of T and fill value
// return true if success
template <typename T> 
bool read (stream & stm, T &value){
	static_assert (std::is_pod<T>::value, "Cannot stream read non-standard layout types");
	auto bytes (stm.sgetn(reinterpret_cast<uint8_t *> (&value), sizeof (value)));
	return bytes == sizeof (value);
}

template <typename T>
bool write (stream & stm, T const & value){
	static_assert (std::is_pod<T>::value, "Cannot stream write non-standard layout types");
	auto bytes (stm.sputn(reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
	assert (bytes == sizeof (value));
	return bytes == sizeof (value);
}


void thisThreadSleepForSeconds(unsigned sec);

}  // namespace taraxa


#endif
