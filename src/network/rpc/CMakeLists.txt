# Note: run make rpc_stubs to re-generate rpc classes
# TODO: refactor this cmake

include(EthDependencies)
include(EthExecutableHelper)

set(CPP_NAMESPACE taraxa::net)

find_program(ETH_JSON_RPC_STUB jsonrpcstub)
add_custom_target(gen_rpc_stubs)
add_custom_target(rpc_stubs)
file(GLOB API_DEF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.jsonrpc.json")
foreach (api_def_file ${API_DEF_FILES})
    get_filename_component(api_name ${api_def_file} NAME_WE)
    jsonrpcstub_create(
            gen_rpc_stubs ${api_name}.jsonrpc.json
            ${CPP_NAMESPACE}::${api_name}Face
            ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Face
            ${CPP_NAMESPACE}::${api_name}Client
            ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Client
    )
    set(server_file ${CMAKE_CURRENT_SOURCE_DIR}/${api_name}Face.h)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${api_name}_fix.cmake "
file(READ ${server_file} server_file_content)
string(REPLACE
       \"include \\\"ModularServer.h\\\"\"
       \"include \\\"network/rpc/ModularServer.h\\\"\"
       server_file_content \"\${server_file_content}\")
file(WRITE ${server_file} \"\${server_file_content}\")
    ")
    add_custom_target(${api_name}_fix COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_BINARY_DIR}/${api_name}_fix.cmake)
    add_dependencies(${api_name}_fix gen_rpc_stubs)
    add_dependencies(rpc_stubs ${api_name}_fix)
endforeach ()
