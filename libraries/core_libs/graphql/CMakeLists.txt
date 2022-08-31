########### cppgraphqlgen ###########
include(FetchContent)

set(Boost_NO_WARN_NEW_VERSIONS 1)

FetchContent_Declare(
  cppgraphqlgen
  GIT_REPOSITORY https://github.com/microsoft/cppgraphqlgen.git
  GIT_TAG        8c1623acc42392ef2a1bc0336482621386f98c77 # v4.5.0
)
set(GRAPHQL_BUILD_TESTS OFF)
set(GRAPHQL_UPDATE_VERSION OFF)
set(GRAPHQL_UPDATE_SAMPLES OFF)
#set(GRAPHQL_BUILD_SCHEMAGEN OFF)
set(GRAPHQL_BUILD_CLIENTGEN OFF)
FetchContent_MakeAvailable(cppgraphqlgen)
######################################

# find_package(cppgraphqlgen CONFIG REQUIRED)

# unifiedschema
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen)

add_custom_command(
        OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TaraxaSchema.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TaraxaSchema.h
        COMMAND cppgraphqlgen::schemagen --schema="${CMAKE_CURRENT_SOURCE_DIR}/schema/schema.taraxa.graphql" --prefix="Taraxa" --namespace="taraxa"
        DEPENDS schema/schema.taraxa.graphql
        WORKING_DIRECTORY gen
        COMMENT "Regenerating TaraxaSchema files")

file(GLOB OLD_GEN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/gen/Taraxa*)

add_custom_command(
        OUTPUT regenerated_schema_files
        COMMAND ${CMAKE_COMMAND} -E remove -f ${OLD_GEN_FILES}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/gen ${CMAKE_CURRENT_SOURCE_DIR}/gen
        COMMAND ${CMAKE_COMMAND} -E touch regenerated_schema_files
        DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TaraxaSchema.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TaraxaSchema.h
        COMMENT "Updating TaraxaSchema files")

add_custom_target(regenerate_schema_files ALL DEPENDS regenerated_schema_files)