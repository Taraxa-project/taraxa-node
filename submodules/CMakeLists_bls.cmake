# Alternative cmake for libBLS
project(libBLS)

set(prefix_libBLS libBLS)

set(sourses_bls
        ${prefix_libBLS}/bls/bls.cpp
        ${prefix_libBLS}/bls/BLSPrivateKeyShare.cpp
        ${prefix_libBLS}/bls/BLSPrivateKey.cpp
        ${prefix_libBLS}/bls/BLSPublicKeyShare.cpp
        ${prefix_libBLS}/bls/BLSPublicKey.cpp
        ${prefix_libBLS}/bls/BLSSignature.cpp
        ${prefix_libBLS}/bls/BLSSigShare.cpp
        ${prefix_libBLS}/bls/BLSSigShareSet.cpp
        ${prefix_libBLS}/dkg/dkg.cpp
        ${prefix_libBLS}/dkg/DKGBLSWrapper.cpp
        ${prefix_libBLS}/dkg/DKGBLSSecret.cpp
        ${prefix_libBLS}/third_party/cryptlite/base64.cpp
        ${prefix_libBLS}/tools/utils.cpp
        )

set(headers_bls
        ${prefix_libBLS}/libBLS.h
        ${prefix_libBLS}/bls/bls.h
        ${prefix_libBLS}/bls/BLSPrivateKeyShare.h
        ${prefix_libBLS}/bls/BLSPrivateKey.h
        ${prefix_libBLS}/bls/BLSPublicKeyShare.h
        ${prefix_libBLS}/bls/BLSPublicKey.h
        ${prefix_libBLS}/bls/BLSSignature.h
        ${prefix_libBLS}/bls/BLSSigShare.h
        ${prefix_libBLS}/bls/BLSSigShareSet.h
        ${prefix_libBLS}/dkg/dkg.h
        ${prefix_libBLS}/dkg/DKGBLSWrapper.h
        ${prefix_libBLS}/dkg/DKGBLSSecret.h
        ${prefix_libBLS}/third_party/json.hpp
        ${prefix_libBLS}/third_party/cryptlite/sha256.h
        ${prefix_libBLS}/third_party/cryptlite/sha1.h
        ${prefix_libBLS}/third_party/cryptlite/hmac.h
        ${prefix_libBLS}/third_party/cryptlite/base64.h
        ${prefix_libBLS}/tools/utils.h
        )

## add new target, because libBLS at submodule have too many properties
## and we can do it here more simpler
add_library(bls STATIC ${sourses_bls} ${headers_bls})
target_link_libraries(bls PUBLIC libff::ff CONAN_PKG::cryptopp ${GMP_LIBRARIES} CONAN_PKG::openssl)
target_include_directories(bls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${prefix_libBLS} ${CONAN_INCLUDE_DIRS_BOOST} ${GMP_INCLUDES} ${libff_inlcude_dir}/..)
target_compile_options(bls PUBLIC -Wall -Wparentheses -Wunused -Wno-error=unused-private-field -Wno-error=vla-extension -Wno-error=extra-semi -Wno-error=sign-compare -Wno-error=sign-compare -Wno-error=bitwise-op-parentheses -Wno-error=deprecated-copy -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=uninitialized -Wunreachable-code -Wextra -fPIC)

# bls tests
add_executable(bls_unit_test ${prefix_libBLS}/test/unit_tests_bls.cpp)
target_link_libraries(bls_unit_test PRIVATE bls)

add_executable(dkg_unit_test ${prefix_libBLS}/test/unit_tests_dkg.cpp)
target_link_libraries(dkg_unit_test PRIVATE bls)

add_executable(bls_test ${prefix_libBLS}/test/test_bls.cpp)
target_link_libraries(bls_test PRIVATE bls)

add_executable(utils_unit_test ${prefix_libBLS}/test/unit_tests_utils.cpp)
target_link_libraries(utils_unit_test PRIVATE bls)

add_custom_target(all_bls_tests
        COMMAND ./bls_unit_test
        COMMAND ./dkg_unit_test
        COMMAND ./bls_test
        COMMAND ./utils_unit_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        COMMENT "Run all BLS and DKG tests"
        )