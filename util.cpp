#include "util.hpp"
namespace taraxa{
rapidjson::Document strToJson(const std::string &json_str){

	rapidjson::Document doc;
	doc.Parse(json_str.c_str());
	if (doc.HasParseError()) {
		throw std::invalid_argument(
			__FILE__ "(" + std::to_string(__LINE__) + ") "
			"Couldn't parse Json data at character position " + std::to_string(doc.GetErrorOffset())
			+ ": " + rapidjson::GetParseError_En(doc.GetParseError())
		);	
	}

	if (!doc.IsObject()) {
		throw std::invalid_argument(
			__FILE__ "(" + std::to_string(__LINE__) + ") "
			"Json data doesn't represent an object ( {...} )"
		);
	}
	return doc;
}

rapidjson::Document loadJsonFile(const std::string& json_file_name) {
	try {
		// read file as string
		std::ifstream json_file(json_file_name);
		std::string json_str;

		json_file.seekg(0, std::ios::end);   
		json_str.reserve(json_file.tellg());
		json_file.seekg(0, std::ios::beg);

		json_str.assign((std::istreambuf_iterator<char>(json_file)),
			std::istreambuf_iterator<char>());
		
		return strToJson(json_str);
		
	} catch (std::invalid_argument const & err){
		throw;
	} catch (std::exception const &err) {
		throw std::invalid_argument(
			__FILE__ "(" + std::to_string(__LINE__) + ") "
			"Couldn't load Json from file " + json_file_name
			+ ": " + err.what()
		);
	} 
}

void thisThreadSleepForSeconds(unsigned sec){
	std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void thisThreadSleepForMilliSeconds(unsigned millisec){
	std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
}

void thisThreadSleepForMicroSeconds(unsigned microsec){
	std::this_thread::sleep_for(std::chrono::microseconds(microsec));
}

}  // namespace taraxa
