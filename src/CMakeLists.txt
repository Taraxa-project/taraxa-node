# TODO: Break this cmake(app_base target) into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

# Find boost required components
set(BOOST_COMPONENTS)
list(APPEND BOOST_COMPONENTS program_options system filesystem)

if (APPLE)
    list(APPEND BOOST_COMPONENTS thread-mt log-mt log_setup-mt)
else()
    list(APPEND BOOST_COMPONENTS thread log log_setup)
endif()

find_package(Boost 1.71 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
if (Boost_FOUND)
    message (STATUS "Found Boost components: ${BOOST_COMPONENTS}, Boost version: ${Boost_VERSION}")
else(Boost_FOUND)
    message(STATUS "Boost not found")
endif (Boost_FOUND)

set(SOURCES
    # ---- public headers -----
    util/util.hpp
    node/full_node.hpp
    transaction_manager/transaction_manager.hpp
    storage/db_storage.hpp
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
    node/executor.hpp
    network/network.hpp
    common/static_init.hpp
    dag/vdf_sortition.hpp
    chain/final_chain.hpp
    consensus/pbft_config.hpp
    network/taraxa_capability.hpp
    util/exit_stack.hpp
    util/simple_event.hpp
    util/encoding_rlp.hpp
    util/range_view.hpp
    util/lazy.hpp
    config/config.hpp
    node/replay_protection_service.hpp
    common/types.hpp
    dag/dag_block.hpp
    network/rpc/Taraxa.h
    network/rpc/Test.h
    network/rpc/TaraxaClient.h
    network/rpc/NetClient.h
    network/rpc/Net.h
    network/rpc/NetFace.h
    network/rpc/TaraxaFace.h
    network/rpc/TestFace.h
    network/rpc/RpcServer.h
    network/rpc/WSServer.h
    network/rpc/TestClient.h
    network/graphql/TaraxaSchemaImpl.h
    transaction_manager/transaction_order_manager.hpp
    consensus/vote.hpp
    transaction_manager/transaction.hpp
    transaction_manager/transaction_queue.hpp
    logger/logger_config.hpp
    logger/log.hpp
    chain/state_api.hpp
    consensus/pbft_manager.hpp
    
    # ---- private sources -----
    transaction_manager/transaction.cpp
    consensus/pbft_manager.cpp
    storage/db_storage.cpp
    consensus/vrf_wrapper.cpp
    aleth/node_api.cpp
    aleth/pending_block.cpp
    aleth/filter_api.cpp
    aleth/database.cpp
    aleth/state_api.cpp
    node/executor.cpp
    network/taraxa_capability.cpp
    transaction_manager/transaction_manager.cpp
    dag/vdf_sortition.cpp
    chain/chain_config.cpp
    chain/final_chain.cpp
    consensus/pbft_config.cpp
    consensus/vote.cpp
    network/network.cpp
    util/util.cpp
    consensus/pbft_chain.cpp
    node/replay_protection_service.cpp
    config/config.cpp
    dag/dag_block.cpp
    node/full_node.cpp
    consensus/block_proposer.cpp
    network/rpc/Taraxa.cpp
    network/rpc/Test.cpp
    network/rpc/WSServer.cpp
    network/rpc/Net.cpp
    network/rpc/RpcServer.cpp
    network/graphql/TaraxaSchemaImpl.cpp
    transaction_manager/transaction_order_manager.cpp
    chain/state_api.cpp
    transaction_manager/transaction_queue.cpp
    dag/dag.cpp
    logger/logger_config.cpp
    logger/log.cpp
    common/types.cpp
)

add_subdirectory(network/rpc)
add_subdirectory(network/graphql)

add_library(app_base STATIC ${SOURCES})
target_include_directories(app_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIR}
    ${Submodules_INCLUDE_DIR}
    /usr/include/jsoncpp # needed only because jsoncpp packaging is not standard across OS, TODO: refactor this hardcoded path to find_package()
)

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
)

target_link_directories(app_base PUBLIC ${Submodules_LIB_DIR})

# needed for golang runtime that comes together with taraxa-evm
if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(app_base "-framework CoreFoundation")
    target_link_libraries(app_base "-framework Security")
else()
    target_link_libraries(app_base stdc++fs)
endif()

if(CLANG_TIDY_EXE)
    set_target_properties(app_base PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
endif()

# Main taraxad binary
add_subdirectory(taraxad)