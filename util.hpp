/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:26:50 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-30 17:27:33
 */
 
 #ifndef UTIL_HPP
 #define UTIL_HPP

#include <iostream>
#include <fstream>
#include <streambuf>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
namespace taraxa{

rapidjson::Document strToJson( std::string const & str);
// load file and convert to json doc
rapidjson::Document loadJsonFile( std::string const& json_file_name); 
}  // namespace taraxa

#endif
