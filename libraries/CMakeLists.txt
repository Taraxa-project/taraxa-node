add_subdirectory(cli)
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(consensus)
add_subdirectory(logger)
add_subdirectory(network)
add_subdirectory(node)
add_subdirectory(storage)
add_subdirectory(types)
add_subdirectory(vdf)

add_subdirectory(aleth)

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
        final_chain
        logger
        network
        node
        storage
        transaction_manager
        taraxa-evm
        pthread
)

if(TCMALLOC_LIB)
    target_link_libraries(app_base INTERFACE ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()
