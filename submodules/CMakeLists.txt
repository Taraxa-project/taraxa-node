# Add secp256k1 & libff
## like aleth do it
include(ProjectSecp256k1)
include(ProjectLibFF)

# Add ethash
set(include_dir ethash/include)
set(prefix_ethash ethash/lib/ethash)
## add new target, because ethash at submodule have too many properties
## and we can do it here more simplier
add_library(
    ethash
    STATIC
    ${prefix_ethash}/bit_manipulation.h
    ${prefix_ethash}/builtins.h
    ${prefix_ethash}/endianness.hpp
    ${include_dir}/ethash/ethash.h
    ${include_dir}/ethash/ethash.hpp
    ${prefix_ethash}/ethash-internal.hpp
    ${prefix_ethash}/ethash.cpp
    ${include_dir}/ethash/hash_types.h
    ${prefix_ethash}/managed.cpp
    ${include_dir}/ethash/keccak.h
    ${include_dir}/ethash/keccak.hpp
    ${prefix_ethash}/keccak.c
    ${prefix_ethash}/keccakf800.c
    ${prefix_ethash}/keccakf1600.c
    ${prefix_ethash}/kiss99.hpp
    ${prefix_ethash}/primes.h
    ${prefix_ethash}/primes.c
    ${include_dir}/ethash/progpow.hpp
    ${prefix_ethash}/progpow.cpp
)
target_include_directories(ethash PUBLIC ${include_dir})

# Add aleth
## create stubs and aliases for aleth targets
find_package(Boost 1.71 REQUIRED system filesystem log thread )

add_library(ethash::ethash ALIAS ethash)

# add_library(libscrypt INTERFACE ${scrypt_LIBRARIES})
# add_library(libscrypt::scrypt ALIAS libscrypt)

add_library(libscrypt::scrypt INTERFACE)
target_link_libraries(libscrypt::scrypt ${scrypt_LIBRARIES})

add_library(jsoncpp_lib_static ALIAS CONAN_PKG::jsoncpp)
add_library(libjson-rpc-cpp::server ALIAS CONAN_PKG::libjson-rpc-cpp)
add_library(cryptopp-static ALIAS CONAN_PKG::cryptopp)

## indicate that aleth build like submodule
set(ALETH_SUBMODULE ON)

# set(CMAKE_CXX_VISIBILITY_PRES  ET default)


## add new targets
add_subdirectory(taraxa-aleth/libdevcore)
add_subdirectory(taraxa-aleth/libdevcrypto)
add_subdirectory(taraxa-aleth/libethcore)
add_subdirectory(taraxa-aleth/libp2p)
add_subdirectory(taraxa-aleth/libethereum)
add_subdirectory(taraxa-aleth/libweb3jsonrpc)

## include directory to avoid compile erros
target_include_directories(devcore PUBLIC "taraxa-aleth")
target_include_directories(devcrypto PUBLIC ${CONAN_INCLUDE_DIRS_CRYPTOPP} secp256k1/include)

# ## set different visibility for devcrypto
# set_target_properties(devcrypto PROPERTIES CXX_VISIBILITY_PRESET default)
# target_compile_options(devcore PRIVATE -Wl --exclude-libs ALL)
# target_link_options(devcore PRIVATE -Wl --exclude-libs ALL)
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)


## create aleth target for just included targets
add_library(aleth INTERFACE)
target_link_libraries(aleth INTERFACE ethereum devcore ethcore devcrypto web3jsonrpc)

## add not cmake target
## libprefix - submodule prefix (examples: vrf/vdf/evm)
## libpath - name of compiled library (examples: libvdf.a/libsodium.a/libtaraxaevm.dylib)
## libincludepath - relative path (after `*/include/`) to headers (examples: ""/""/tarxa-evm)
## buildcommand - build command of library (example: "${CMAKE_MAKE_PROGRAM} OPENSSL_HOME=${CONAN_OPENSSL_ROOT} all")
function(add_none_cmake_target libprefix libname buildcommand)
    # split argument because it goes as string
    separate_arguments(MAKE_COMMAND UNIX_COMMAND ${buildcommand})
    # set the output destination
    set(TARAXA_${libprefix}_LIB ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-${libprefix}/lib/${libname})
    # create a custom target called build_taraxa_${libprefix}
    add_custom_target(build_taraxa_${libprefix} 
                      COMMAND ${MAKE_COMMAND}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-${libprefix}/
                      COMMENT "Original taraxa-${libprefix} makefile target")

    # now create an imported static target
    add_library(taraxa-${libprefix} STATIC IMPORTED GLOBAL)
    # Import target "taraxa-${libprefix}" for configuration ""
    set_property(TARGET taraxa-${libprefix} APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
    set_target_properties(taraxa-${libprefix} PROPERTIES IMPORTED_LOCATION_NOCONFIG "${TARAXA_${libprefix}_LIB}")
    set_target_properties(taraxa-${libprefix} PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/taraxa-${libprefix}/include")
    set_target_properties(taraxa-${libprefix} PROPERTIES ADDITIONAL_CLEAN_FILES "${TARAXA_${libprefix}_LIB}")

    # now you can use taraxa-${libprefix} as if it were a regular cmake built target in your project
    add_dependencies(taraxa-${libprefix} build_taraxa_${libprefix})
endfunction()

# Add taraxa-vdf
add_none_cmake_target(vdf libvdf.a "${CMAKE_MAKE_PROGRAM} OPENSSL_HOME=${CONAN_OPENSSL_ROOT} all")


# Add taraxa-vrf
## run autotools
set(VRF_AUTOTOOLS_CMD "autoreconf && automake")
## run configure
set(VRF_AUTOTOOLS_CMD "${VRF_AUTOTOOLS_CMD} && ./configure --enable-static=\"yes\" --enable-shared=\"no\" --prefix=${CMAKE_CURRENT_SOURCE_DIR}/taraxa-vrf")
add_none_cmake_target(vrf libsodium.a "${VRF_AUTOTOOLS_CMD} && ${CMAKE_MAKE_PROGRAM} && ${CMAKE_MAKE_PROGRAM} install")


# Add tarxa-evm
## go env settings
set(EVM_BUILD_ENV "CGO_CFLAGS_ALLOW='.*' CGO_CXXFLAGS_ALLOW='.*' CGO_CFLAGS=\"-O3\" CGO_CXXFLAGS=\"-O3\"")
## add to search list dirs with libraries from conan
set(EVM_BUILD_LD "-L${CONAN_LIB_DIRS_BZIP2} -L${CONAN_LIB_DIRS_ZLIB} -L${CONAN_LIB_DIRS_ROCKSDB} -L${CONAN_LIB_DIRS_SNAPPY} -L${CONAN_LIB_DIRS_LZ4} -L${CONAN_LIB_DIRS_ZSTD}")
## if we need full static build use flag
if(NOT APPLE)
    set(EVM_BUILD_LD "${EVM_BUILD_LD} -static")
    ## name of the static library
    set(EVM_LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}taraxaevm${CMAKE_STATIC_LIBRARY_SUFFIX}")
else()
    ## name of the shared library
    set(EVM_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}taraxaevm${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()
## add libraries to link
set(EVM_BUILD_LD "${EVM_BUILD_LD} -lrocksdb -lbz2 -lz -lsnappy -llz4 -lzstd")
## add build command
set(EVM_BUILD_COMMAND "cd taraxa/C && ${EVM_BUILD_ENV} CGO_LDFLAGS=\"${EVM_BUILD_LD}\" go build -buildmode=c-archive -o ${EVM_LIBRARY_NAME}")
## cp headers into `taraxa-evm/include`
set(EVM_AFTER_BUILD_COMMAND "cd - && mkdir -p include/taraxa-evm && cp taraxa/C/*.h include/taraxa-evm/.")
## cp library into `taraxa-evm/lib`
set(EVM_AFTER_BUILD_COMMAND "${EVM_AFTER_BUILD_COMMAND} && mkdir -p lib && cp taraxa/C/${EVM_LIBRARY_NAME} lib/.")
## rename header `libtaraxaevm.h` into `taraxa-evm.h`
set(EVM_AFTER_BUILD_COMMAND "${EVM_AFTER_BUILD_COMMAND} && mv include/taraxa-evm/libtaraxaevm.h include/taraxa-evm/taraxa-evm.h")
## final command
set(EVM_LIBRARY_COMMAND "${EVM_BUILD_COMMAND} && ${EVM_AFTER_BUILD_COMMAND}")

add_none_cmake_target(evm "${EVM_LIBRARY_NAME}" "${EVM_LIBRARY_COMMAND}")
