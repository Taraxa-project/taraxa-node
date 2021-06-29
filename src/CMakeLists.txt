add_subdirectory(cli)
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(consensus)
add_subdirectory(dag)
add_subdirectory(final_chain)
add_subdirectory(logger)
add_subdirectory(network)
add_subdirectory(node)
add_subdirectory(storage)
add_subdirectory(transaction_manager)

add_subdirectory(aleth)

# empty.cpp file needed to make this a normal lib(shared or static), but not the interface
add_library(app_base INTERFACE)

# system libs should be included via -isystem so compiler knows how to
# treat them. e.g. do not check for warnings
target_include_directories(app_base SYSTEM INTERFACE
        ${CONAN_INCLUDE_DIRS}
        ${jsoncpp_INCLUDE_DIRS}
)

target_link_libraries(app_base INTERFACE
        cli
        common
        config
        consensus
        dag
        final_chain
        logger
        network
        node
        storage
        transaction_manager
        aleth
        taraxa-evm
        taraxa-vrf
        taraxa-vdf
        pthread
        CONAN_PKG::jsoncpp
        CONAN_PKG::libjson-rpc-cpp
        CONAN_PKG::boost
        CONAN_PKG::openssl
        CONAN_PKG::gmp
        CONAN_PKG::lz4
        CONAN_PKG::rocksdb
        CONAN_PKG::bzip2
        CONAN_PKG::zlib
        CONAN_PKG::mpfr
)

# needed for golang runtime that comes together with taraxa-evm
if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(app_base INTERFACE "-framework CoreFoundation")
    target_link_libraries(app_base INTERFACE "-framework Security")
else ()
    target_link_libraries(app_base INTERFACE stdc++fs)
endif ()

if(TCMALLOC_LIB)
    target_link_libraries(app_base INTERFACE ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

install(TARGETS app_base
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

# Main taraxad binary
add_subdirectory(taraxad)
