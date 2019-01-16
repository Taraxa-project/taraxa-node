/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-15 11:38:38 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-15 12:59:29
 */
 
#include "types.hpp"
namespace taraxa{
uint256_hash_t::uint256_hash_t(std::string const & str){
	decodeHex(str);
}
uint256_hash_t::uint256_hash_t(const char *cstr){
	decodeHex(std::string(cstr));
}
uint256_hash_t::uint256_hash_t(uint256_t const & number){
	uint256_t dup (number);
	for (auto i (bytes.rbegin()), e (bytes.rend()); i != e; ++i){
		*i = static_cast<uint8_t> (dup & static_cast<uint8_t> (0xff));
		dup >>= 8;
	}
}
bool uint256_hash_t::decodeHex(std::string const & str){
	auto ok (true);
	if (!str.empty() && str.size() <= 64){
		std::stringstream stream (str);
		stream << std::hex << std::noshowbase;
		uint256_t number;
		try{
			stream >> number;
			*this = number;
			if (!stream.eof()) {
				ok=false;
			}
		}
		catch (...) {
			ok = false;
		}
	}
	else {
		ok = false;
	}
	assert(ok);
	return ok;
}

void uint256_hash_t::encodeHex(std::string &str) const {
	assert(str.empty());
	std::stringstream stream;
	stream<<std::hex<<std::noshowbase<<std::setw(64)<<std::setfill('0');
	stream << number();
	str = stream.str();
}

uint256_t uint256_hash_t::number () const {
	uint256_t res;
	for (auto i (bytes.begin()), e (bytes.end()); i != e; ++i){
		res <<= 8;
		res |= *i;
	}
	return res;
}
void uint256_hash_t::clear() {
	for (auto i (bytes.begin()), e (bytes.end()); i != e; ++i){
		*i=static_cast<uint8_t> (0);
	}
}
void uint256_hash_t::operator=(std::string const & str){
	decodeHex(str);
}

bool uint256_hash_t::operator== (uint256_hash_t const & other) const{
	return bytes == other.bytes;
}


std::ostream & operator<<(std::ostream & strm, uint256_hash_t const &num){
	strm << std::hex<<num.toString() <<" ";
	return strm;
}

std::string uint256_hash_t::toString() const{
	std::string res;
	encodeHex(res);
	return res;
}

void uint256_hash_t::rawPrint() const{
	for (auto i (bytes.begin()), e(bytes.end()); i != e; ++i){
		std::cout<<std::bitset<8>(*i)<<" ";
	}
	std::cout<<std::endl;
}

} // namespace
