/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:26:50 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-10 12:33:26
 */
 
 #ifndef UTIL_HPP
 #define UTIL_HPP

#include <iostream>
#include <fstream>
#include <streambuf>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
namespace taraxa{
typedef std::string key_t;
typedef std::string name_t;
typedef uint64_t	bal_t;
typedef std::string sig_t;
typedef std::string blk_hash_t;
typedef std::string nonce_t;

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
