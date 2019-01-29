/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-15 11:38:38 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-29 11:26:54
 */
 
#include "types.hpp"
namespace taraxa{

template <std::size_t Bytes> 
uint_hash_t<Bytes>::uint_hash_t(std::string const & str){
	decodeHex(str);
}

template <std::size_t Bytes> 
uint_hash_t<Bytes>::uint_hash_t(const char *cstr){
	decodeHex(std::string(cstr));
}

template <std::size_t Bytes> 
uint_hash_t<Bytes>::uint_hash_t(Number const & number){
	typename uint_hash_t<Bytes>::Number dup (number);
	for (auto i (bytes.rbegin()), e (bytes.rend()); i != e; ++i){
		*i = static_cast<uint8_t> (dup & static_cast<uint8_t> (0xff));
		dup >>= 8;
	}
}

template <std::size_t Bytes>
bool uint_hash_t<Bytes>::decodeHex(std::string const & str){
	auto ok (true);
	if (str.size() == bytes.size()*2){
		std::stringstream stream (str);
		stream << std::hex << std::noshowbase;
		typename uint_hash_t<Bytes>::Number number;
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
		std::cerr<<"uint_hash_t type size is "<<bytes.size()*2<<" chars, "
			<<"but input string has length "<<str.size()<<" "<<str<<std::endl;
		assert(false);
		ok = false;
	}
	assert(ok);
	return ok;
}

template <std::size_t Bytes>
void uint_hash_t<Bytes>::encodeHex(std::string &str) const {
	unsigned size = bytes.size()*2;
	assert(str.empty());
	std::stringstream stream;
	stream<<std::hex<<std::noshowbase<<std::setw(size)<<std::setfill('0');
	stream << number();
	str = stream.str();
}

template <std::size_t Bytes>
typename uint_hash_t<Bytes>::Number uint_hash_t<Bytes>::number () const {
	typename uint_hash_t::Number res;
	for (auto i (bytes.begin()), e (bytes.end()); i != e; ++i){
		res <<= 8;
		res |= *i;
	}
	return res;
}

template <std::size_t Bytes>
void uint_hash_t<Bytes>::clear() {
	for (auto i (bytes.begin()), e (bytes.end()); i != e; ++i){
		*i=static_cast<uint8_t> (0);
	}
}

template <std::size_t Bytes>
void uint_hash_t<Bytes>::operator=(std::string const & str){
	decodeHex(str);
}

template <std::size_t Bytes>
void uint_hash_t<Bytes>::operator=(const char *str){
	decodeHex(std::string(str));
}

template <std::size_t Bytes>
bool uint_hash_t<Bytes>::operator== (uint_hash_t<Bytes> const & other) const{
	return bytes == other.bytes;
}

template <std::size_t Bytes>
bool uint_hash_t<Bytes>::operator< (uint_hash_t<Bytes> const & other) const{
	return bytes < other.bytes;
}

template <std::size_t Bytes>
std::ostream & operator<<(std::ostream & strm, uint_hash_t<Bytes> const &num){
	strm << std::hex<<num.toString() <<" ";
	return strm;
}

template <std::size_t Bytes>
std::string uint_hash_t<Bytes>::toString() const{
	std::string res;
	encodeHex(res);
	return res;
}

template <std::size_t Bytes>
void uint_hash_t<Bytes>::rawPrint() const{
	for (auto i (bytes.begin()), e(bytes.end()); i != e; ++i){
		std::cout<<std::bitset<8>(*i)<<" ";
	}
	std::cout<<std::endl;
}

// create instance to be linked in other sourc files

template class uint_hash_t<32>;
template class uint_hash_t<64>;
template std::ostream & operator<<(std::ostream & strm, uint_hash_t<32> const &num);
template std::ostream & operator<<(std::ostream & strm, uint_hash_t<64> const &num);


} // namespace
