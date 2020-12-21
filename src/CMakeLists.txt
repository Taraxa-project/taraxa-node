cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    # ---- public headers -----
    util.hpp
    util_test/gtest.hpp
    util_test/util.hpp
    util_test/samples.hpp
    full_node.hpp
    transaction_manager.hpp
    db_storage.hpp
    dag.hpp
    pbft_chain.hpp
    aleth/node_api.hpp
    aleth/filter_api.hpp
    aleth/database.hpp
    aleth/pending_block.hpp
    aleth/state_api.hpp
    util_json.hpp
    block_proposer.hpp
    transaction_status.hpp
    chain_config.hpp
    vrf_wrapper.hpp
    executor.hpp
    network.hpp
    static_init.hpp
    vdf_sortition.hpp
    final_chain.hpp
    pbft_config.hpp
    sortition.hpp
    taraxa_capability.hpp
    util/exit_stack.hpp
    util/simple_event.hpp
    util/encoding_rlp.hpp
    util/range_view.hpp
    util/lazy.hpp
    config.hpp
    replay_protection_service.hpp
    types.hpp
    dag_block.hpp
    net/Taraxa.h
    net/Test.h
    net/TaraxaClient.h
    net/NetClient.h
    net/Net.h
    net/NetFace.h
    net/TaraxaFace.h
    net/TestFace.h
    net/graphql/TaraxaSchemaImpl.h
    net/RpcServer.h
    net/WSServer.h
    net/TestClient.h
    transaction_order_manager.hpp
    vote.hpp
    transaction.hpp
    transaction_queue.hpp
    logger/Config.hpp
    logger/log.hpp
    state_api.hpp
    pbft_manager.hpp
    
    # ---- private sources -----
    sortition.cpp
    transaction.cpp
    pbft_manager.cpp
    db_storage.cpp
    vrf_wrapper.cpp
    aleth/node_api.cpp
    aleth/pending_block.cpp
    aleth/filter_api.cpp
    aleth/database.cpp
    aleth/state_api.cpp
    executor.cpp
    taraxa_capability.cpp
    transaction_manager.cpp
    vdf_sortition.cpp
    chain_config.cpp
    final_chain.cpp
    pbft_config.cpp
    final_chain_test.cpp
    replay_protection_service_test.cpp
    vote.cpp
    state_api_test.cpp
    network.cpp
    util.cpp
    pbft_chain.cpp
    replay_protection_service.cpp
    config.cpp
    dag_block.cpp
    full_node.cpp
    block_proposer.cpp
    net/Taraxa.cpp
    net/Test.cpp
    net/WSServer.cpp
    net/Net.cpp
    net/RpcServer.cpp
    net/graphql/TaraxaSchemaImpl.cpp
    transaction_order_manager.cpp
    state_api.cpp
    transaction_queue.cpp
    dag.cpp
    logger/Config.cpp
    logger/log.cpp
    types.cpp
)

# TODO: is this needed anymore ???
#set(IS_DEBUG 0)
#if (CMAKE_BUILD_TYPE MATCHES Debug)
#    set(IS_DEBUG 1)
#endif ()

add_library(app_base STATIC ${SOURCES})
target_include_directories(app_base PUBLIC .) # todo: refactor
target_include_directories(app_base PUBLIC /usr/include/jsoncpp)   # needed only because jsoncpp packaging is not standard across OS

# todo: refactor hardcoded submodules/include and submodules/lib
target_include_directories(app_base PUBLIC ${CMAKE_BINARY_DIR}/submodules/include)
target_link_directories(app_base PUBLIC ${CMAKE_BINARY_DIR}/submodules/lib)

add_subdirectory(net/graphql)

# if not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
if (APPLE)
    target_link_libraries(app_base "-framework CoreFoundation")
    target_link_libraries(app_base "-framework Security")
endif()

target_compile_definitions(app_base PUBLIC
    CRYPTOPP_DISABLE_ASM
#    BOOST_ALL_DYN_LINK
    BOOST_SPIRIT_THREADSAFE
    GIT_HASH="\"$(GIT_HASH)\""
    COMPILE_TIME="\"$(COMPILE_TIME)\""
)

# TODO: is this needed anymore ???
#ifndef COMPILE_FLAGS
#    ifeq ($(DEBUG), 1)
#        COMPILE_FLAGS := -g -O0
#    else
#        COMPILE_FLAGS := -O3
#    endif
#endif
#
#ifndef LINK_FLAGS
#    LINK_FLAGS := -Wl,-rpath $(DEPS_INSTALL_PREFIX)/lib
#    ifeq ($(DEBUG), 1)
#        ifeq ($(OS), Darwin)
#            LINK_FLAGS += -rdynamic
#        else
#            LINK_FLAGS += -Wl,--export-dynamic
#        endif
#    endif
#endif

SET(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS program_options
    system
    filesystem
)

if (APPLE)
    LIST(APPEND BOOST_COMPONENTS thread-mt
        log-mt
        log_setup-mt)
else()
    LIST(APPEND BOOST_COMPONENTS thread
        log
        log_setup)
endif()

find_package(Boost 1.71 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
message("BOOST_COMPONENTS: ${BOOST_COMPONENTS}")

message("Boost_FOUND: ${Boost_FOUND}")

target_link_libraries(app_base
    submodules
    taraxa_graphqlservice
    taraxa-aleth
    taraxa-evm
    vdf
    sodium
    dl
    pthread
    z
    curl
    ssl
    crypto
    gmp
    gmpxx
    mpfr
    ${Boost_LIBRARIES}
    rocksdb
    scrypt
    jsoncpp
    jsonrpccpp-common
    jsonrpccpp-server
    ff
    secp256k1
    cryptopp
    ethash
    stdc++fs
)

target_include_directories(app_base PUBLIC ${Boost_INCLUDE_DIR})

## Optional linking for libatomic (part of standard library).
## Some toolchains provide this library,
## and assume programs using <atomic> would link against it.
## Note: makefile translates `$$?` into `$?`
#LIBATOMIC_NOT_FOUND = $(shell \
#$(CXX) $(LIB_DIRS) -latomic -shared -o /dev/null &> /dev/null; echo $$? \
#)
#ifeq ($(LIBATOMIC_NOT_FOUND), 0)
#LIBS += atomic
#endif

add_executable(main main.cpp)
target_link_libraries(main app_base)