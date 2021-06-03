# Note: run make rpc_stubs to re-generate rpc classes

include(${CMAKE_SOURCE_DIR}/submodules/taraxa-aleth/cmake/EthDependencies.cmake)
include(${CMAKE_SOURCE_DIR}/submodules/taraxa-aleth/cmake/EthExecutableHelper.cmake)

find_program(ETH_JSON_RPC_STUB jsonrpcstub)

set(CPP_NAMESPACE taraxa::net)
file(GLOB API_DEF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.jsonrpc.json")
add_custom_target(rpc_stubs)
foreach (api_def_file ${API_DEF_FILES})
    get_filename_component(api_name ${api_def_file} NAME_WE)
    jsonrpcstub_create(
            rpc_stubs ${api_name}.jsonrpc.json
            ${CPP_NAMESPACE}::${api_name}Face
            ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Face
            ${CPP_NAMESPACE}::${api_name}Client
            ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Client
    )
endforeach ()
