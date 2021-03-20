#pragma once

#include <openssl/evp.h>
#include <memory>
#include <vector>
#include <utility>
#include "zallocator.hpp"

namespace vdf
{
using byte = unsigned char;
using byte_ = byte;
using byte_ptr = std::unique_ptr<byte>;
using bytevec = std::vector<byte>;
using SecureString = std::basic_string<byte, std::char_traits<byte>, zallocator<byte>>;
using EVP_CIPHER_CTX_free_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&::EVP_CIPHER_CTX_free)>;
using BN_CTX_free_ptr = std::unique_ptr<BN_CTX, decltype(&::BN_CTX_free)>;
using ProofWesolowski = bytevec;
using ProofPietrzak = std::vector<bytevec>;

template <typename proof>
using Solution = std::pair<proof, bytevec>;

using SolutionWesolowski = Solution<ProofWesolowski>;
using SolutionPietrzak = Solution<ProofPietrzak>;

} // namespace vdf