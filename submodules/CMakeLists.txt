set(
        __COMPILE_DEFINITIONS__
        BOOST_SPIRIT_THREADSAFE
        CRYPTOPP_DISABLE_ASM
)
if (TARAXA_STATIC_BUILD)
    list(APPEND __COMPILE_DEFINITIONS__ BOOST_ALL_STATIC_LINK)
else ()
    list(APPEND __COMPILE_DEFINITIONS__ BOOST_ALL_DYN_LINK)
endif ()
# TODO: get rid of submodules Makefile...
add_custom_target(
        submodules_build
        COMMAND
        $(MAKE)
        UPDATE_SUBMODULES=${UPDATE_SUBMODULES}
        DIST=${CMAKE_CURRENT_BINARY_DIR}
        SYSTEM_HOME_OVERRIDE=${SYSTEM_HOME_OVERRIDE}
        COMPILE_DEFINITIONS="${__COMPILE_DEFINITIONS__}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_library(submodules INTERFACE)
add_dependencies(submodules submodules_build)
target_compile_definitions(submodules INTERFACE ${__COMPILE_DEFINITIONS__})
target_include_directories(submodules INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_directories(submodules INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/lib)
target_link_libraries(
        submodules
        INTERFACE
        taraxa-aleth
        taraxa-evm
        vdf     # from current submodules -> will be moved to external package repo
        sodium  # from current submodules(taraxa-vrf) -> will be moved to external package repo
        secp256k1   # from current submodules -> will be moved to external package repo
        cryptopp    # from current submodules -> will be moved to external package repo
        ethash      # from current submodules -> will be moved to external package repo
)
