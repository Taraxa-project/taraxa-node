cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    # ---- public headers -----
    ../util.hpp
    ../util_test/gtest.hpp
    ../util_test/util.hpp
    ../util_test/samples.hpp
    ../full_node.hpp
    ../transaction_manager.hpp
    ../db_storage.hpp
    ../dag.hpp
    ../pbft_chain.hpp
    ../aleth/node_api.hpp
    ../aleth/filter_api.hpp
    ../aleth/database.hpp
    ../aleth/pending_block.hpp
    ../aleth/state_api.hpp
    ../util_json.hpp
    ../block_proposer.hpp
    ../transaction_status.hpp
    ../chain_config.hpp
    ../vrf_wrapper.hpp
    ../executor.hpp
    ../network.hpp
    ../static_init.hpp
    ../vdf_sortition.hpp
    ../final_chain.hpp
    ../pbft_config.hpp
    ../sortition.hpp
    ../taraxa_capability.hpp
    ../util/exit_stack.hpp
    ../util/simple_event.hpp
    ../util/encoding_rlp.hpp
    ../util/range_view.hpp
    ../util/lazy.hpp
    ../config.hpp
    ../replay_protection_service.hpp
    ../types.hpp
    ../dag_block.hpp
    ../net/Taraxa.h
    ../net/Test.h
    ../net/TaraxaClient.h
    ../net/NetClient.h
    ../net/Net.h
    ../net/NetFace.h
    ../net/TaraxaFace.h
    ../net/TestFace.h
    ../net/graphql/TaraxaSchemaImpl.h
    ../net/RpcServer.h
    ../net/WSServer.h
    ../net/TestClient.h
    ../transaction_order_manager.hpp
    ../vote.hpp
    ../transaction.hpp
    ../transaction_queue.hpp
    ../logger/Config.hpp
    ../logger/log.hpp
    ../state_api.hpp
    ../pbft_manager.hpp
    
    # ---- private sources -----
    ../sortition.cpp
    ../transaction.cpp
    ../pbft_manager.cpp
    ../db_storage.cpp
    ../vrf_wrapper.cpp
    ../aleth/node_api.cpp
    ../aleth/pending_block.cpp
    ../aleth/filter_api.cpp
    ../aleth/database.cpp
    ../aleth/state_api.cpp
    ../executor.cpp
    ../taraxa_capability.cpp
    ../transaction_manager.cpp
    ../vdf_sortition.cpp
    ../chain_config.cpp
    ../final_chain.cpp
    ../pbft_config.cpp
    ../final_chain_test.cpp
    ../replay_protection_service_test.cpp
    ../vote.cpp
    ../state_api_test.cpp
    ../network.cpp
    ../util.cpp
    ../pbft_chain.cpp
    ../replay_protection_service.cpp
    ../config.cpp
    ../dag_block.cpp
    ../full_node.cpp
    ../block_proposer.cpp
    ../net/Taraxa.cpp
    ../net/Test.cpp
    ../net/WSServer.cpp
    ../net/Net.cpp
    ../net/RpcServer.cpp
    ../net/graphql/TaraxaSchemaImpl.cpp
    ../transaction_order_manager.cpp
    ../state_api.cpp
    ../transaction_queue.cpp
    ../dag.cpp
    ../logger/Config.cpp
    ../logger/log.cpp
    ../types.cpp
)

set(IS_DEBUG 0)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(IS_DEBUG 1)
endif ()

macro(read_makefile_var var_name)
    execute_process(
        COMMAND
        make DEBUG=${IS_DEBUG} UPDATE_SUBMODULES=0 print_${var_name}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE ${var_name}
    )
    string(REPLACE " " ";" ${var_name} ${${var_name}})
    list(TRANSFORM ${var_name} STRIP)
endmacro()

#read_makefile_var(COMPILE_DEFINITIONS)
#add_compile_definitions(${COMPILE_DEFINITIONS})

add_compile_definitions(
    CRYPTOPP_DISABLE_ASM
    BOOST_ALL_DYN_LINK
    BOOST_SPIRIT_THREADSAFE
    GIT_HASH="\"$(GIT_HASH)\""
    COMPILE_TIME="\"$(COMPILE_TIME)\""
)

# refactor for target_include_directories
include_directories(/usr/include/jsoncpp)   # needed only because jsoncpp packaging is not standard across OS
include_directories(${CMAKE_BINARY_DIR}/deps/include)

link_directories(${CMAKE_BINARY_DIR}/deps/lib)

read_makefile_var(LIB_DIRS)
link_directories(${LIB_DIRS})
message("OMG: " ${LIB_DIRS})

read_makefile_var(LINK_FLAGS)
add_link_options(${LINK_FLAGS})


add_library(app_base STATIC ${SOURCES})
target_include_directories(app_base PUBLIC ../)
target_include_directories(app_base PUBLIC /usr/include/jsoncpp)   # needed only because jsoncpp packaging is not standard across OS
target_include_directories(app_base PUBLIC ${CMAKE_BINARY_DIR}/deps/include)

target_link_libraries(app_base submodules taraxa_graphqlservice)

read_makefile_var(LIBS)
read_makefile_var(OSX_FRAMEWORKS)
list(TRANSFORM OSX_FRAMEWORKS PREPEND "-framework ")
target_link_libraries(app_base ${LIBS} ${OSX_FRAMEWORKS})

#read_makefile_var(NODE_SRCS)
#add_library(node_objects OBJECT ${NODE_SRCS})
#add_executable(main $<TARGET_OBJECTS:node_objects> src/main.cpp)
#target_link_libraries(main submodules)

# An extension of this file that you can play with locally
include(local/CmakeLists_ext.cmake OPTIONAL)