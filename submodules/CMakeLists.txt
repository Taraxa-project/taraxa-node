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

# Add scrypt 
add_subdirectory(libscrypt)
include_directories(libscrypt)

# Add aleth
add_subdirectory(taraxa-aleth)

include(ExternalProject)

## add not cmake target
## libprefix - submodule prefix (examples: vrf/vdf/evm)
## libpath - name of compiled library (examples: libvdf.a/libsodium.a/libtaraxaevm.dylib)
## libincludepath - relative path (after `*/include/`) to headers (examples: ""/""/tarxa-evm)
## buildcommand - build command of library (example: "${CMAKE_MAKE_PROGRAM} OPENSSL_HOME=${CONAN_OPENSSL_ROOT} all")
function(add_make_target libprefix libname buildcommand)
    # prefix of build dir
    set(prefix "${CMAKE_BINARY_DIR}/deps")
    # write it to file
    # because if we use it as it is we will have an error with enviroment vars
    # they will be interpreted in the wrong key (CGO_CFLAGS_ALLOW='.*' goes as "CGO_CFLAGS_ALLOW=.*")
    set(${libprefix}_SHFILE ${prefix}/setenv_for_taraxa_${libprefix}.sh)
    file(WRITE ${${libprefix}_SHFILE} "${buildcommand}")
    # library path
    set(${libprefix}_LIBRARY "${prefix}/taraxa-${libprefix}/lib/${libname}")
    # inlcude path
    set(${libprefix}_INCLUDE_DIR "${prefix}/taraxa-${libprefix}/include")
    ExternalProject_Add(
        ${libprefix}_lib_submodule
        PREFIX ${prefix}
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-${libprefix}
        BINARY_DIR ${prefix}/taraxa-${libprefix}
        CONFIGURE_COMMAND cp -r <SOURCE_DIR> ${prefix}
        BUILD_COMMAND chmod +x ${${libprefix}_SHFILE} && bash ${${libprefix}_SHFILE}
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${${libprefix}_LIBRARY}
    )

    add_library(taraxa-${libprefix} STATIC IMPORTED GLOBAL)
    file(MAKE_DIRECTORY "${${libprefix}_INCLUDE_DIR}")  # Must exist.
    set_property(TARGET taraxa-${libprefix} PROPERTY IMPORTED_CONFIGURATIONS Release)
    set_property(TARGET taraxa-${libprefix} PROPERTY IMPORTED_LOCATION_RELEASE "${${libprefix}_LIBRARY}")
    set_property(TARGET taraxa-${libprefix} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${${libprefix}_INCLUDE_DIR}")
    add_dependencies(taraxa-${libprefix} ${libprefix}_lib_submodule)
endfunction()

# Add taraxa-vdf
set(VDF_LIBRARY_COMMAND "\
 ${CMAKE_MAKE_PROGRAM}\
 CPPFLAGS=\"-I./include -I${CONAN_INCLUDE_DIRS_OPENSSL} -I${CONAN_INCLUDE_DIRS_GMP} -I${CONAN_INCLUDE_DIRS_MPFR} -std=c++17 -O3\"\
 lib/libvdf.a")
add_make_target(vdf libvdf.a "${VDF_LIBRARY_COMMAND}")


# Add taraxa-vrf
## run autotools
set(VRF_AUTOTOOLS_CMD "autoreconf && automake")
## run configure
set(VRF_AUTOTOOLS_CMD "${VRF_AUTOTOOLS_CMD} && ./configure --enable-static=\"yes\" --enable-shared=\"no\" --prefix=${CMAKE_BINARY_DIR}/deps/taraxa-vrf")
add_make_target(vrf libsodium.a "${VRF_AUTOTOOLS_CMD} && ${CMAKE_MAKE_PROGRAM} && ${CMAKE_MAKE_PROGRAM} install")


# Add taraxa-evm
## add include of libs
set(EVM_BUILD_INCLUDE -I${CONAN_INCLUDE_DIRS_BZIP2} -I${CONAN_INCLUDE_DIRS_ZLIB} -I${CONAN_INCLUDE_DIRS_ROCKSDB} -I${CONAN_INCLUDE_DIRS_SNAPPY} -I${CONAN_INCLUDE_DIRS_LZ4} -I${CONAN_INCLUDE_DIRS_ZSTD})
## set C flags
set(EVM_BUILD_CGO_CFLAGS -O3 ${EVM_BUILD_INCLUDE})
## add link of libs
set(EVM_BUILD_LD -L${CONAN_LIB_DIRS_BZIP2} -lbz2 -L${CONAN_LIB_DIRS_ZLIB} -lz -L${CONAN_LIB_DIRS_ROCKSDB} -lrocksdb -L${CONAN_LIB_DIRS_SNAPPY} -lsnappy -L${CONAN_LIB_DIRS_LZ4} -llz4 -L${CONAN_LIB_DIRS_ZSTD} -lzstd)
## if we need full static build use flag
if(TARAXA_STATIC_BUILD)
    set(EVM_BUILD_LD ${EVM_BUILD_LD} -static)
    ## name of the static library
    set(EVM_LIBRARY_NAME ${CMAKE_STATIC_LIBRARY_PREFIX}taraxaevm${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    ## name of the shared library
    set(EVM_LIBRARY_NAME ${CMAKE_SHARED_LIBRARY_PREFIX}taraxaevm${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()
## go env settings
set(EVM_BUILD_ENV CGO_CFLAGS_ALLOW='.*' CGO_CXXFLAGS_ALLOW='.*' CGO_CXXFLAGS=\"-O3\" CGO_CFLAGS=\"${EVM_BUILD_CGO_CFLAGS}\" CGO_LDFLAGS=\"${EVM_BUILD_LD}\")
## add build command
set(EVM_BUILD_COMMAND cd taraxa/C && ${EVM_BUILD_ENV} go build -buildmode=c-archive -o ${EVM_LIBRARY_NAME})
## cp headers into `taraxa-evm/include`
set(EVM_AFTER_BUILD_COMMAND cd - && mkdir -p include/taraxa-evm && cp taraxa/C/*.h include/taraxa-evm/.)
## cp library into `taraxa-evm/lib`
set(EVM_AFTER_BUILD_COMMAND ${EVM_AFTER_BUILD_COMMAND} && mkdir -p lib && cp taraxa/C/${EVM_LIBRARY_NAME} lib/.)
## rename header `libtaraxaevm.h` into `taraxa-evm.h`
set(EVM_AFTER_BUILD_COMMAND ${EVM_AFTER_BUILD_COMMAND} && mv include/taraxa-evm/libtaraxaevm.h include/taraxa-evm/taraxa-evm.h)
## final command
set(EVM_LIBRARY_COMMAND ${EVM_BUILD_COMMAND} && ${EVM_AFTER_BUILD_COMMAND})

file(GLOB_RECURSE TARAXA_EVM_SOURCES "taraxa-evm/*.go" )
list(APPEND TARAXA_EVM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/taraxa/C/common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/taraxa/C/state.h)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/lib/${EVM_LIBRARY_NAME}
    DEPENDS ${TARAXA_EVM_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm
    COMMAND ${EVM_LIBRARY_COMMAND}
    COMMENT "Building taraxa-evm library")

add_custom_target(taraxa_evm_build DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/lib/${EVM_LIBRARY_NAME})

add_library(taraxa-evm INTERFACE)
add_dependencies(taraxa-evm taraxa_evm_build)
target_link_libraries(taraxa-evm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/lib/${EVM_LIBRARY_NAME})
target_include_directories(taraxa-evm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/taraxa-evm/include)
