/*
Copyright 2018 Ilja Honkonen
*/

#ifndef HASHES_HPP
#define HASHES_HPP

#include "bin2hex2bin.hpp"

#include <cryptopp/config.h>

namespace taraxa {

/*
Returns hash of given binary payload.

Argument and output are binary.

Usually Chars = std::string and Hasher = CryptoPP::BLAKE2s.
*/
template<class Hasher, class Chars> Chars get_hash_bin(const Chars& payload) {
	Hasher hasher;
	Chars digest_bin(Hasher::DIGESTSIZE, 0);
	hasher.CalculateDigest(
		reinterpret_cast<CryptoPP::byte*>(digest_bin.data()),
		reinterpret_cast<const CryptoPP::byte*>(payload.data()),
		payload.size()
	);
	return digest_bin;
}


//! As get-hash_bin but argument and result are hex encoded without leading 0x.
template<class Hasher, class Chars> Chars get_hash_hex(const Chars& payload) {
	return bin2hex(get_hash_bin(hex2bin(payload)));
}

} // namespace taraxa

#endif // ifndef HASHES_HPP
