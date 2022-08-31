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


set(HEADERS
    gen/TaraxaSchema.h
    include/graphql/account.hpp
    include/graphql/block.hpp
    include/graphql/log.hpp
    include/graphql/query.hpp
    include/graphql/transaction.hpp
    include/graphql/mutation.hpp
    include/graphql/ws_server.hpp
    # include/graphql/http_processor.hpp
    include/graphql/subscription.hpp
    include/graphql/types/dag_block.hpp
    include/graphql/types/current_state.hpp
)

set(SOURCES
    gen/TaraxaSchema.cpp
    src/account.cpp
    src/block.cpp
    src/log.cpp
    src/query.cpp
    src/transaction.cpp
    src/mutation.cpp
    src/ws_server.cpp
    # src/http_processor.cpp
    src/subscription.cpp
    src/types/dag_block.cpp
    src/types/current_state.cpp
)

add_library(graphql ${SOURCES} ${HEADERS})
add_dependencies(graphql regenerate_schema_files)

target_include_directories(graphql PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gen)
target_include_directories(graphql PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(graphql PUBLIC cppgraphqlgen::graphqlservice cppgraphqlgen::graphqljson core_libs)

install(TARGETS graphql
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
