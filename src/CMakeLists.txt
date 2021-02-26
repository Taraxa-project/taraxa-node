# TODO: Break this cmake into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries

if (TARAXA_STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # zlib does not support zlib_USE_STATIC_LIBS or similar approach
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")

    set(Boost_USE_STATIC_LIBS ON)
    set(jsoncpp_USE_STATIC_LIBS ON)
    set(jsonrpccpp_USE_STATIC_LIBS ON)
    set(crypto_USE_STATIC_LIBS ON)
    set(gmp_USE_STATIC_LIBS ON)
    set(mpfr_USE_STATIC_LIBS ON)
    set(scrypt_USE_STATIC_LIBS ON)
    set(rocksdb_USE_STATIC_LIBS ON)
    set(zstd_USE_STATIC_LIBS ON)
    set(bz2_USE_STATIC_LIBS ON)
    set(snappy_USE_STATIC_LIBS ON)
    set(lz4_USE_STATIC_LIBS ON)
endif (TARAXA_STATIC_BUILD)

# Find multithreaded boost components/libs
set(Boost_USE_MULTITHREADED ON)
set(BOOST_COMPONENTS)
list(APPEND BOOST_COMPONENTS program_options system filesystem thread log log_setup)

find_package(Boost 1.71 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
if (Boost_FOUND)
    message(STATUS "Boost components found: ${BOOST_COMPONENTS}, Boost version: ${Boost_VERSION}")
else (Boost_FOUND)
    message(STATUS "Boost not found")
endif (Boost_FOUND)

# Find jsoncpp lib
find_package(jsoncpp REQUIRED)
find_package(jsonrpccpp REQUIRED)
find_package(crypto REQUIRED) # Find crypto lib # TODO: check if really required
find_package(gmp REQUIRED)
find_package(mpfr REQUIRED)
find_package(scrypt REQUIRED)
find_package(rocksdb REQUIRED)
find_package(zstd REQUIRED)
find_package(bz2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(snappy REQUIRED)
find_package(lz4 REQUIRED)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.hpp *.cxx *.hxx *.c *.h)
list(REMOVE_ITEM SOURCES taraxad/main.cpp)

add_subdirectory(network/rpc)

add_library(base_config INTERFACE)

set(PCH_FILE ${CMAKE_CURRENT_BINARY_DIR}/pch.hpp)
pch_gen(${CMAKE_CURRENT_SOURCE_DIR} ${PCH_FILE} jsonrpccpp/client.h)
pch_link(base_config INTERFACE ${PCH_FILE})


target_include_directories(base_config INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${Boost_INCLUDE_DIR}
        ${jsoncpp_INCLUDE_DIRS}
        )

target_link_libraries(base_config INTERFACE
        submodules
        pthread
        #ssl
        ${crypto_LIBRARIES} # TODO: is this lib even required ?
        ${CMAKE_DL_LIBS}    # because of libcrypto
        ${mpfr_LIBRARIES}
        ${gmp_LIBRARIES}
        ${Boost_LIBRARIES}
        ${rocksdb_LIBRARIES}
        ${snappy_LIBRARIES} # rocksdb dep
        ${zstd_LIBRARIES}   # rocksdb dep
        ${bz2_LIBRARIES}    # rocksdb dep
        ${ZLIB_LIBRARIES}   # rocksdb dep
        ${lz4_LIBRARIES}    # rocksdb dep
        ${scrypt_LIBRARIES}
        ${jsoncpp_LIBRARIES}
        ${jsonrpccpp_SERVER_LIBRARIES}
        )

# needed for golang runtime that comes together with taraxa-evm
if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(base_config INTERFACE
            "-framework CoreFoundation"
            "-framework Security")
else ()
    target_link_libraries(base_config INTERFACE stdc++fs)
endif ()

add_library(app_base OBJECT ${SOURCES})
target_link_libraries(app_base base_config)

# Main taraxad binary
add_subdirectory(taraxad)