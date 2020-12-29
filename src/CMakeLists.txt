cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    # ---- public headers -----
        util/util.hpp
    full_node.hpp
        transaction_manager/transaction_manager.hpp
    db_storage.hpp
        dag/dag.hpp
        consensus/pbft_chain.hpp
    aleth/node_api.hpp
    aleth/filter_api.hpp
    aleth/database.hpp
    aleth/pending_block.hpp
    aleth/state_api.hpp
        util/util_json.hpp
        consensus/block_proposer.hpp
        transaction_manager/transaction_status.hpp
        chain/chain_config.hpp
        consensus/vrf_wrapper.hpp
        consensus/executor.hpp
    network.hpp
    static_init.hpp
        dag/vdf_sortition.hpp
        chain/final_chain.hpp
        consensus/pbft_config.hpp
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
        dag/dag_block.hpp
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
        transaction_manager/transaction_order_manager.hpp
        consensus/vote.hpp
        transaction_manager/transaction.hpp
        transaction_manager/transaction_queue.hpp
    logger/config.hpp
    logger/log.hpp
        state_api.hpp
        consensus/pbft_manager.hpp
    
    # ---- private sources -----
    sortition.cpp
        transaction_manager/transaction.cpp
        consensus/pbft_manager.cpp
    db_storage.cpp
        consensus/vrf_wrapper.cpp
    aleth/node_api.cpp
    aleth/pending_block.cpp
    aleth/filter_api.cpp
    aleth/database.cpp
    aleth/state_api.cpp
        consensus/executor.cpp
    taraxa_capability.cpp
        transaction_manager/transaction_manager.cpp
        dag/vdf_sortition.cpp
        chain/chain_config.cpp
        chain/final_chain.cpp
        consensus/pbft_config.cpp
        consensus/vote.cpp
    network.cpp
        util/util.cpp
        consensus/pbft_chain.cpp
    replay_protection_service.cpp
    config.cpp
        dag/dag_block.cpp
    full_node.cpp
        consensus/block_proposer.cpp
    net/Taraxa.cpp
    net/Test.cpp
    net/WSServer.cpp
    net/Net.cpp
    net/RpcServer.cpp
    net/graphql/TaraxaSchemaImpl.cpp
        transaction_manager/transaction_order_manager.cpp
        state_api.cpp
        transaction_manager/transaction_queue.cpp
        dag/dag.cpp
    logger/config.cpp
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
LIST(APPEND BOOST_COMPONENTS program_options system filesystem)

if (APPLE)
    LIST(APPEND BOOST_COMPONENTS thread-mt log-mt log_setup-mt)
else()
    LIST(APPEND BOOST_COMPONENTS thread log log_setup)
endif()

find_package(Boost 1.71 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
if (Boost_FOUND)
    message (STATUS "Found Boost components: ${BOOST_COMPONENTS}, Boost version: ${Boost_VERSION}")
else(Boost_FOUND)
    MESSAGE(STATUS "Boost not found")
endif (Boost_FOUND)

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

add_subdirectory(taraxad)