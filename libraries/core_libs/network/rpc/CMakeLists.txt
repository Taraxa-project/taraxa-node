# Note: run make gen_rpc_stubs to re-generate rpc classes

# include(EthDependencies)
# include(EthExecutableHelper)

# find_program(ETH_JSON_RPC_STUB jsonrpcstub)

# set(CPP_NAMESPACE taraxa::net)
# file(GLOB API_DEF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.jsonrpc.json")
# add_custom_target(gen_rpc_stubs)
# foreach (api_def_file ${API_DEF_FILES})
#     get_filename_component(api_name ${api_def_file} NAME_WE)
#     jsonrpcstub_create(
#             gen_rpc_stubs ${api_name}.jsonrpc.json
#             ${CPP_NAMESPACE}::${api_name}Face
#             ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Face
#             ${CPP_NAMESPACE}::${api_name}Client
#             ${CMAKE_CURRENT_SOURCE_DIR} ${api_name}Client
#     )
# endforeach ()
