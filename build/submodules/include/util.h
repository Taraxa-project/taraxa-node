#pragma once

#include <openssl/bn.h>
#include <string>
#include "../include/types.h"

namespace vdf
{
bytevec bn2bytevec(const BIGNUM *in);
void bn2bytevec(const BIGNUM *in, bytevec &out);
std::string print_bn(const BIGNUM *bn);
std::string print_bn_hex(const BIGNUM *bn);
std::string print_bytevec(const bytevec &b);
}; // namespace vdf
