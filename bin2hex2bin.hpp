/*
Copyright 2018 Ilja Honkonen
*/

#ifndef BIN2HEX2BIN_HPP
#define BIN2HEX2BIN_HPP


#include <cryptopp/hex.h>


namespace taraxa {

/*
Returns hex representation of binary data @bin.

Doesn't return the leading "0x".

Usually Chars = std::string.
*/
template<class Chars> Chars bin2hex(const Chars& bin) {
	CryptoPP::HexEncoder encoder;

	encoder.Put(reinterpret_cast<const CryptoPP::byte*>(bin.data()), bin.size());
	encoder.MessageEnd();

	Chars hex;
	hex.resize(encoder.MaxRetrievable());
	encoder.Get(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(hex.data())), hex.size());

	return hex;
}


/*
Returns binary data transformed from hex representation in @hex.

@hex must not contain the leading "0x".
Silently skips characters outside of hex alphabet.
Ignores last char if given odd number of chars.

Usually Chars = std::string.
*/
template<class Chars> Chars hex2bin(const Chars& hex) {
	CryptoPP::HexDecoder decoder;

	decoder.Put(reinterpret_cast<const CryptoPP::byte*>(hex.data()), hex.size());
	decoder.MessageEnd();

	Chars bin;
	bin.resize(decoder.MaxRetrievable());
	decoder.Get(reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(bin.data())), bin.size());

	return bin;
}

} // namespace taraxa

#endif // ifndef BIN2HEX2BIN_HPP
