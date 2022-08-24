set(HEADERS
    include/cli/config.hpp
    include/cli/config_updater.hpp
    include/cli/tools.hpp
)

set(SOURCES
    src/config.cpp
    src/config_updater.cpp
    src/tools.cpp
)

add_library(cli ${SOURCES} ${HEADERS})
target_include_directories(cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cli PUBLIC config)

install(TARGETS cli
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set(JSONS_DIRECTIORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cli/config_jsons)

file(READ ${JSONS_DIRECTIORY}/default.json DEFAULT_CONFIG_JSON)
file(READ ${JSONS_DIRECTIORY}/mainnet.json MAINNET_CONFIG_JSON)
file(READ ${JSONS_DIRECTIORY}/testnet.json TESTNET_CONFIG_JSON)
file(READ ${JSONS_DIRECTIORY}/devnet.json DEVNET_CONFIG_JSON)

# no maps in cmake, so it is organized as two arrays. Order of this and config_files should be the same
set(configs
    DEFAULT_CONFIG_JSON
    MAINNET_CONFIG_JSON
    TESTNET_CONFIG_JSON
    DEVNET_CONFIG_JSON
)
set(config_files
    ${JSONS_DIRECTIORY}/default.json
    ${JSONS_DIRECTIORY}/mainnet.json
    ${JSONS_DIRECTIORY}/testnet.json
    ${JSONS_DIRECTIORY}/devnet.json
)

# this helps to trigger configuration step after change of JSON conffiguration files 
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${config_files})

list(LENGTH configs configs_count)
# length should be less by 1 for foreach
math(EXPR configs_count "${configs_count}-1")
foreach(i RANGE ${configs_count})
    # get file path and configuration variable name
    list(GET configs ${i} config_name)
    list(GET config_files ${i} config_file)

    # read json from file to corresponding variable
    file(READ ${config_file} ${config_name})
    set(config_str ${${config_name}})

    # check json format. we are getting type of `chain_config` field here, 
    # but if we will have any issue with json format it will be reported to `json_error` variable
    string(JSON result ERROR_VARIABLE json_error TYPE ${config_str} chain_config)
    if(NOT(${result} STREQUAL "OBJECT"))
        message(FATAL_ERROR "Can't parse ${config_name}: ${json_error}")
    endif()
endforeach()

# generate proper header file from json configuration files data read before
configure_file(${JSONS_DIRECTIORY}/configs.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/cli/configs.hpp)