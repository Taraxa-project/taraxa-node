/*
Copyright 2018 Ilja Honkonen
*/

#ifndef GENERATE_PRIVATE_KEY_FROM_SEED_HPP
#define GENERATE_PRIVATE_KEY_FROM_SEED_HPP

#include <string>
#include <vector>

namespace taraxa{
	std::vector<std::string> generate_private_key_from_seed(
		const std::string &seed, 
		size_t first, 
		size_t last
	);
}
#endif
