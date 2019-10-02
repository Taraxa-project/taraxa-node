/*
Modified from https://github.com/fcelda/nsec5-crypto/blob/486e3114b8986b1342ff06f3131985e3360f32f9/demo_ecvrf_p256.c
*/

#include <array>
#include <cassert>
#include <cstdbool>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include <arpa/inet.h>
//#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#include "hashes.hpp"
#include "signatures.hpp"

/**
 * EC VRF suite.
 */
struct ecvrf_suite {
	EC_GROUP *group;
	const EVP_MD *hash;
	const size_t proof_size;
	const size_t ecp_size;
	const size_t c_size;
	const size_t s_size;
};

/**
 * Get EC-VRF-P256-SHA256 implementation.
 */
static ecvrf_suite *ecvrf_p256()
{
	ecvrf_suite tmp{
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1),
		EVP_sha256(),
		81,
		33,
		16,
		32,
	};

	if (!tmp.group) {
		return nullptr;
	}

	auto* result = (ecvrf_suite*) std::malloc(sizeof(ecvrf_suite));
	if (result == nullptr) {
		return nullptr;
	}

	std::memcpy(result, &tmp, sizeof(ecvrf_suite));
	return result;
}

/**
 * Free EC VRF implementation.
 */
static void ecvfr_free(struct ecvrf_suite *suite)
{
	if (!suite) {
		return;
	}

	EC_GROUP_free(suite->group);
	free(suite);
}

/**
 * Get number of bytes that fit given number of bits.
 *
 * ceil(div(bits/8))
 */
static int bits_in_bytes(int bits)
{
	return (bits + 7) / 8;
}

/**
 * Encode unsigned integer on a fixed width.
 */
static void bn2bin(const BIGNUM *num, uint8_t *buf, size_t size)
{
	size_t need = BN_num_bytes(num);
	assert(need <= size);

	size_t pad = size - need;
	if (pad > 0) {
		memset(buf, 0, pad);
	}

	size_t ret = BN_bn2bin(num, buf + pad);
	if (ret != need) {
		throw std::runtime_error(__FILE__ "(" + std::to_string(__LINE__) + ")");
	}
}

/**
 * BN_mod_mul without context.
 *
 * OpenSSL BN_mod_mul segfaults without BN_CTX.
 */
static int bn_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
{
	BN_CTX *ctx = BN_CTX_new();
	if (!ctx) {
		return 0;
	}

	int ret = BN_mod_mul(r, a, b, m, ctx);

	BN_CTX_free(ctx);

	return ret;
}

/**
 * Compute r = p1^f1 + p2^f2
 */
static EC_POINT *ec_mul_two(const EC_GROUP *group, const EC_POINT *p1, const BIGNUM *f1, const EC_POINT *p2, const BIGNUM *f2)
{
	EC_POINT *result = EC_POINT_new(group);
	if (!result) {
		return NULL;
	}

	const EC_POINT *points[] = { p1, p2 };
	const BIGNUM *factors[] = { f1, f2 };
	if (EC_POINTs_mul(group, result, NULL, 2, points, factors, NULL) != 1) {
		EC_POINT_clear_free(result);
		return NULL;
	}

	return result;
}

/**
 * Try converting random string to EC point.
 *
 * @return EC point or NULL if the random string cannot be interpreted as an EC point.
 */
static EC_POINT *RS2ECP(const EC_GROUP *group, const uint8_t *data, size_t size)
{
	std::vector<uint8_t> buffer(size + 1, 0);
	buffer[0] = 0x02;
	memcpy(buffer.data() + 1, data, size);

	EC_POINT *point = EC_POINT_new(group);
	if (EC_POINT_oct2point(group, point, buffer.data(), buffer.size(), NULL) == 1) {
		return point;
	} else {
		EC_POINT_clear_free(point);
		return NULL;
	}
}

/**
 * Convert hash value to an EC point.
 *
 * This implementation will work for any curve but execution is not time-constant.
 *
 * @return EC point or NULL in case of failure.
 */
static EC_POINT *ECVRF_hash_to_curve1(const ecvrf_suite *vrf, const EC_POINT *pubkey, const uint8_t *data, size_t size)
{
	if (pubkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return nullptr;
	}
	int degree = bits_in_bytes(EC_GROUP_get_degree(vrf->group));
	std::vector<uint8_t> _pubkey(degree + 1, 0);
	if (EC_POINT_point2oct(vrf->group, pubkey, POINT_CONVERSION_COMPRESSED, _pubkey.data(), _pubkey.size(), NULL) != _pubkey.size()) {
		return nullptr;
	}

	EC_POINT *result = nullptr;

	EVP_MD_CTX *md_template = EVP_MD_CTX_new();
	if (!md_template) {
		return nullptr;
	}
	EVP_DigestInit_ex(md_template, vrf->hash, NULL);
	EVP_DigestUpdate(md_template, _pubkey.data(), _pubkey.size());
	EVP_DigestUpdate(md_template, data, size);

	EVP_MD_CTX *md = EVP_MD_CTX_new();
	if (!md) {
		EVP_MD_CTX_free(md_template);
		return nullptr;
	}

	for (uint32_t _counter = 0; result == nullptr || EC_POINT_is_at_infinity(vrf->group, result); _counter++) {
		assert(_counter < 256); // hard limit for debugging
		uint32_t counter = htonl(_counter);
		static_assert(sizeof(counter) == 4, "counter is 4-byte");

		uint8_t hash[EVP_MAX_MD_SIZE] = {0};
		unsigned hash_size = sizeof(hash);

		EVP_DigestInit_ex(md, vrf->hash, NULL);
		EVP_MD_CTX_copy_ex(md, md_template);
		EVP_DigestUpdate(md, &counter, sizeof(counter));
		if (EVP_DigestFinal_ex(md, hash, &hash_size) != 1) {
			EC_POINT_clear_free(result);
			result = nullptr;
			break;
		}

		// perform multiplication with cofactor if cofactor is > 1
		const BIGNUM *cofactor = EC_GROUP_get0_cofactor(vrf->group);
		assert(cofactor);
		result = RS2ECP(vrf->group, hash, hash_size);
		if (result != nullptr && !BN_is_one(cofactor)) {
			EC_POINT *tmp = EC_POINT_new(vrf->group);
			if (EC_POINT_mul(vrf->group, tmp, NULL, result, cofactor, NULL) != 1) {
				EC_POINT_clear_free(tmp);
				EC_POINT_clear_free(result);
				result = nullptr;
				break;
			}
			EC_POINT_clear_free(result);
			result = tmp;
		}
	}

	EVP_MD_CTX_free(md);
	EVP_MD_CTX_free(md_template);

	return result;
}

/**
 * Hash several EC points into an unsigned integer.
 */
static BIGNUM *ECVRF_hash_points(const ecvrf_suite *vrf, const EC_POINT **points, size_t count)
{
	BIGNUM *result = nullptr;

	EVP_MD_CTX *md = EVP_MD_CTX_new();
	if (!md) {
		return nullptr;
	}
	EVP_DigestInit_ex(md, vrf->hash, NULL);

	for (size_t i = 0; i < count; i++) {
		std::vector<uint8_t> buffer(vrf->ecp_size, 0);
		if (EC_POINT_point2oct(vrf->group, points[i], POINT_CONVERSION_COMPRESSED, buffer.data(), buffer.size(), NULL) != buffer.size()) {
			return nullptr;
		}
		EVP_DigestUpdate(md, buffer.data(), buffer.size());
	}

	uint8_t hash[EVP_MAX_MD_SIZE] = {0};
	unsigned hash_size = sizeof(hash);
	if (EVP_DigestFinal_ex(md, hash, &hash_size) != 1) {
		return nullptr;
	}

	assert(hash_size >= vrf->c_size);
	result = BN_bin2bn(hash, vrf->c_size, NULL);
	EVP_MD_CTX_free(md);

	return result;
}

/**
 * Construct ECVRF proof.
 *
 * @param[in]  group
 * @param[in]  pubkey
 * @param[in]  privkey
 * @param[in]  data
 * @param[in]  size
 * @param[out] proof
 * @param[in]  proof_size
 */
static bool ECVRF_prove(
	const ecvrf_suite *vrf, const EC_POINT *pubkey, const BIGNUM *privkey,
	const uint8_t *data, size_t size,
	uint8_t *proof, size_t /*proof_size*/)
{
	// TODO: check input constraints

	if (vrf == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return false;
	}

	if (privkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return false;
	}

	if (pubkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return false;
	}

	if (data == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return false;
	}

	if (proof == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return false;
	}

	const EC_POINT *generator = EC_GROUP_get0_generator(vrf->group);
	assert(generator);
	const BIGNUM *order = EC_GROUP_get0_order(vrf->group);
	assert(order);

	EC_POINT *hash = NULL;
	EC_POINT *gamma = NULL;
	EC_POINT *g_k = NULL;
	EC_POINT *h_k = NULL;
	BIGNUM *nonce = NULL;
	BIGNUM *c = NULL;
	BIGNUM *cx = NULL;
	BIGNUM *s = NULL;

	hash = ECVRF_hash_to_curve1(vrf, pubkey, data, size);
	if (!hash) {
		return false;
	}

	gamma = EC_POINT_new(vrf->group);
	if (EC_POINT_mul(vrf->group, gamma, NULL, hash, privkey, NULL) != 1) {
		return false;
	}

	nonce = BN_new();
	if (BN_rand_range(nonce, order) != 1) {
		return false;
	}

	g_k = EC_POINT_new(vrf->group);
	if (EC_POINT_mul(vrf->group, g_k, NULL, generator, nonce, NULL) != 1) {
		return false;
	}

	h_k = EC_POINT_new(vrf->group);
	if (EC_POINT_mul(vrf->group, h_k, NULL, hash, nonce, NULL) != 1) {
		return false;
	}

	const EC_POINT *points[] = { generator, hash, pubkey, gamma, g_k, h_k };
	c = ECVRF_hash_points(vrf, points, sizeof(points) / sizeof(EC_POINT *));
	if (!c) {
		return false;
	}

	cx = BN_new();
	if (bn_mod_mul(cx, c, privkey, order) != 1) {
		return false;
	}

	s = BN_new();
	if (BN_mod_sub(s, nonce, cx, order, NULL) != 1) {
		return false;
	}

	// write result
	int wrote = EC_POINT_point2oct(vrf->group, gamma, POINT_CONVERSION_COMPRESSED, proof, vrf->ecp_size, NULL);
	if (wrote < 0) {
		return false;
	}
	assert(size_t(wrote) == vrf->ecp_size);
	(void)wrote;
	bn2bin(c, proof + vrf->ecp_size, vrf->c_size);
	bn2bin(s, proof + vrf->ecp_size + vrf->c_size, vrf->s_size);

	EC_POINT_clear_free(hash);
	EC_POINT_clear_free(gamma);
	EC_POINT_clear_free(g_k);
	EC_POINT_clear_free(h_k);
	BN_clear_free(nonce);
	BN_clear_free(c);
	BN_clear_free(cx);
	BN_clear_free(s);

	return true;
}

/**
 * ECVRF proof decoding.
 */
static bool ECVRF_decode_proof(
	const ecvrf_suite *vrf, const uint8_t *proof, size_t size,
	EC_POINT **gamma_ptr, BIGNUM **c_ptr, BIGNUM **s_ptr)
{
	if (size != vrf->proof_size) {
		return false;
	}

	assert(vrf->ecp_size + vrf->c_size + vrf->s_size == size);

	const uint8_t *gamma_raw = proof;
	const uint8_t *c_raw = gamma_raw + vrf->ecp_size;
	const uint8_t *s_raw = c_raw + vrf->c_size;
	assert(s_raw + vrf->s_size == proof + size);

	EC_POINT *gamma = EC_POINT_new(vrf->group);
	if (EC_POINT_oct2point(vrf->group, gamma, gamma_raw, vrf->ecp_size, NULL) != 1) {
		EC_POINT_clear_free(gamma);
		return false;
	}

	BIGNUM *c = BN_bin2bn(c_raw, vrf->c_size, NULL);
	if (!c) {
		EC_POINT_clear_free(gamma);
		return false;
	}

	BIGNUM *s = BN_bin2bn(s_raw, vrf->s_size, NULL);
	if (!s) {
		EC_POINT_clear_free(gamma);
		BN_clear_free(c);
		return false;
	}

	*gamma_ptr = gamma;
	*c_ptr = c;
	*s_ptr = s;

	return true;
}

static bool ECVRF_verify(
	const ecvrf_suite *vrf, const EC_POINT *pubkey,
	const uint8_t *data, size_t size,
	const uint8_t *proof, size_t proof_size)
{
	EC_POINT *gamma = NULL;
	EC_POINT *u = NULL;
	EC_POINT *v = NULL;
	BIGNUM *c = NULL;
	BIGNUM *s = NULL;
	BIGNUM *c2 = NULL;

	if (!ECVRF_decode_proof(vrf, proof, proof_size, &gamma, &c, &s)) {
		return false;
	}

	const EC_POINT *generator = EC_GROUP_get0_generator(vrf->group);
	assert(generator);

	EC_POINT *hash = ECVRF_hash_to_curve1(vrf, pubkey, data, size);
	assert(hash);

	u = ec_mul_two(vrf->group, pubkey, c, generator, s);
	if (!u) {
		return false;
	}

	v = ec_mul_two(vrf->group, gamma, c, hash, s);
	if (!u) {
		return false;
	}

	const EC_POINT *points[] = {generator, hash, pubkey, gamma, u, v};
	c2 = ECVRF_hash_points(vrf, points, sizeof(points) / sizeof(EC_POINT *));
	if (!c2) {
		return false;
	}

	bool valid = (BN_cmp(c, c2) == 0);

	EC_POINT_clear_free(gamma);
	EC_POINT_clear_free(u);
	EC_POINT_clear_free(v);
	BN_clear_free(c);
	BN_clear_free(s);
	BN_clear_free(c2);

	return valid;
}

static void hex_dump(const uint8_t *data, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		bool last = i + 1 == size;
		printf("%02x%c", (unsigned int)data[i], last ? '\n' : ':');
	}
}

static void hex_dump(const std::string& data)
{
	for (size_t i = 0; i < data.size(); i++) {
		bool last = i + 1 == data.size();
		printf("%02x%c", (unsigned int)data[i], last ? '\n' : ':');
	}
}

int main(int argc, char* argv[])
{
	//bool verbose = false;
	std::string message_hex, ledger_path_str, exp_hex;

	boost::program_options::options_description options(
		"Participates in VRF selection of winner for given message\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		//("verbose", "Print more information during execution")
		("message", boost::program_options::value<std::string>(&message_hex),
			"Message (hex) for which to participate in VRF winner selection")
		/*TODO: ("ledger-path",
			boost::program_options::value<std::string>(&ledger_path_str),
			"Ledger data is located in directory arg")*/
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private exponent of the key used for VRF (hex)");

	boost::program_options::variables_map option_variables;
	boost::program_options::store(
		boost::program_options::parse_command_line(argc, argv, options),
		option_variables
	);
	boost::program_options::notify(option_variables);

	if (option_variables.count("help") > 0) {
		std::cout << options << std::endl;
		return EXIT_SUCCESS;
	}

	/*if (option_variables.count("verbose") > 0) {
		verbose = true;
	}*/

	if (exp_hex.size() != 64) {
		std::cerr << "Key must be 64 characters but is " << exp_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	/*boost::filesystem::path ledger_path(ledger_path_str);

	if (ledger_path.size() > 0) {
		if (verbose and not boost::filesystem::portable_directory_name(ledger_path_str)) {
			std::cout << "WARNING: ledger-path isn't portable." << std::endl;
		}

		if (not boost::filesystem::exists(ledger_path)) {
			if (verbose) {
				std::cout << "Ledger directory doesn't exist, creating..." << std::endl;
			}

			try {
				boost::filesystem::create_directories(ledger_path);
			} catch (const std::exception& e) {
				std::cerr << "Couldn't create ledger directory '" << ledger_path
					<< "': " << e.what() << std::endl;
				return EXIT_FAILURE;
			}
		}

		if (not boost::filesystem::is_directory(ledger_path)) {
			std::cerr << "Ledger path isn't a directory: " << ledger_path << std::endl;
			return EXIT_FAILURE;
		}
	}*/

	// create subdirectory for vrf data
	/*auto vrf_path = ledger_path;
	vrf_path /= "vrf";
	if (not boost::filesystem::exists(vrf_path)) {
		try {
			if (verbose) {
				std::cout << "VRF directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directory(vrf_path);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't create a directory for vrf: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}*/

	const auto privkey_bin = taraxa::get_public_key_hex(exp_hex);

	ecvrf_suite* vrf = ecvrf_p256();
	if (!vrf) {
		fprintf(stderr, "failed to create VRF context\n");
		return EXIT_FAILURE;
	}

	EC_KEY* new_key = EC_KEY_new();
	if (new_key == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	if (EC_KEY_set_group(new_key, vrf->group) != 1) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	BIGNUM* privkey = nullptr;;
	if (BN_hex2bn(&privkey, exp_hex.data()) == 0) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}
	if (privkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << std::endl;
	if (EC_KEY_set_private_key(new_key, privkey) != 1) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	BN_CTX* ctx = BN_CTX_new();
	if (ctx == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	EC_POINT* pubkey = EC_POINT_new(vrf->group);
	if (pubkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	if (EC_POINT_mul(vrf->group, pubkey, privkey, nullptr, nullptr, ctx) != 1) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	if (pubkey == nullptr) {
		std::cerr << __FILE__ "(" << __LINE__ << ")" << std::endl;
		return EXIT_FAILURE;
	}

	std::string proof(vrf->proof_size, ' ');

	const std::string
		message_bin = taraxa::hex2bin(message_hex),
		// normalize message hex
		message_hex_out = taraxa::bin2hex(message_bin);

	if (!ECVRF_prove(vrf, pubkey, privkey, (const unsigned char*)message_bin.data(), message_bin.size(), (unsigned char*)proof.data(), proof.size())) {
		fprintf(stderr, "failed to create VRF proof\n");
		return EXIT_FAILURE;
	}

	bool valid = ECVRF_verify(vrf, pubkey, (unsigned char*)message_bin.data(), message_bin.size(), (unsigned char*)proof.data(), proof.size());
	if (not valid) {
		std::cout << "Invalid VRF proof" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string vrf_hash_hex = taraxa::get_hash_hex<CryptoPP::BLAKE2s>(proof);
	std::cout << "VRF output for message " << message_hex_out << ": " << vrf_hash_hex << std::endl;

	EC_KEY_free(new_key);
	ecvfr_free(vrf);

	return EXIT_SUCCESS;
}
