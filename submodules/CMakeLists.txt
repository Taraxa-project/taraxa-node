cmake_minimum_required(VERSION 3.16)

set(
    __COMPILE_DEFINITIONS__
    BOOST_ALL_NO_LIB
    BOOST_ALL_STATIC_LINK
    BOOST_SPIRIT_THREADSAFE
    CRYPTOPP_DISABLE_ASM
)
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

#set(LIB_DEPS_INSTALL_DIR "/usr/local/lib" CACHE PATH "Libraries install directory")
#install(DIRECTORY ${Submodules_LIB_DIR}/ DESTINATION ${LIB_DEPS_INSTALL_DIR} COMPONENT submodules)
