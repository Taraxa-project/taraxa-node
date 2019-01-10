/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:26:50 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-14 15:47:57
 */
 
 #ifndef UTIL_HPP
 #define UTIL_HPP

#include <iostream>
#include <fstream>
#include <streambuf>
#include <string>
#include <boost/asio.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include "types.hpp"
namespace taraxa{



rapidjson::Document strToJson( std::string const & str);
// load file and convert to json doc
rapidjson::Document loadJsonFile( std::string const& json_file_name); 
}  // namespace taraxa

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
	 taraxa::bal_t balance;
};

#endif
