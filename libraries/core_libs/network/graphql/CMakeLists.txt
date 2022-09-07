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

add_custom_command(OUTPUT graphqlgen_schema_files
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/gen/*.cpp
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/gen/*.h
        COMMAND cppgraphqlgen::schemagen --schema="${CMAKE_CURRENT_SOURCE_DIR}/schema/schema.taraxa.graphql" --prefix="Taraxa" --namespace="taraxa"
        DEPENDS schema/schema.taraxa.graphql
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gen
        COMMENT "Regenerating TaraxaSchema files")

add_custom_target(regenerate_schema_files ALL DEPENDS graphqlgen_schema_files)