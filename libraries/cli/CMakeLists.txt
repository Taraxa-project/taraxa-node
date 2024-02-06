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

# no maps in cmake, so it is organized as two arrays. Order of this and config_files should be the same
set(configs
    default
    mainnet
    testnet
    devnet
)

function(CHECK_JSON CONFIG_VAR_NAME FILE_PATH SEARCH_MEMBER)
  # read json from file to corresponding variable
  file(READ ${FILE_PATH} ${CONFIG_VAR_NAME})
  set(CONFIG_STR ${${CONFIG_VAR_NAME}})
  set(${CONFIG_VAR_NAME} ${${CONFIG_VAR_NAME}} PARENT_SCOPE)
  
  # check json format. we are getting type of `SEARCH_MEMBER` field here, 
  # but if we will have any issue with json format it will be reported to `JSON_ERROR` variable
  string(JSON result ERROR_VARIABLE JSON_ERROR TYPE ${CONFIG_STR} ${SEARCH_MEMBER})
  if(NOT(${result} STREQUAL "OBJECT"))
      message(FATAL_ERROR "Can't parse ${CONFIG_VAR_NAME}: ${JSON_ERROR}")
  endif()
endfunction()

# this helps to trigger configuration step after change of JSON conffiguration files 
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${config_directories})

list(LENGTH configs configs_count)
# length should be less by 1 for foreach
math(EXPR configs_count "${configs_count}-1")
foreach(i RANGE ${configs_count})
    # get file path and configuration variable name
    list(GET configs ${i} config)
    string(TOUPPER ${config} config_upper)
    set(config_dir ${JSONS_DIRECTIORY}/${config})

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${config_dir}/${config}_config.json)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${config_dir}/${config}_genesis.json)

    CHECK_JSON(${config_upper}_CONFIG_JSON ${config_dir}/${config}_config.json network)
    CHECK_JSON(${config_upper}_GENESIS_JSON ${config_dir}/${config}_genesis.json dpos)
    # endif()
endforeach()

# generate proper header file from json configuration files data read before
configure_file(${JSONS_DIRECTIORY}/configs.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/cli/configs.hpp)