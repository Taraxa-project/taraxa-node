cmake_minimum_required(VERSION 3.8.2)

find_package(pegtl 3.0.0 CONFIG REQUIRED)
find_package(cppgraphqlgen 3.3.0 CONFIG REQUIRED)

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

file(GLOB OLD_GEN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/gen/*)

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

add_library(taraxa_graphqlservice STATIC
        gen/TaraxaSchema.h
        TaraxaSchemaImpl.h

        gen/TaraxaSchema.cpp
        TaraxaSchemaImpl.cpp
        )
target_link_libraries(taraxa_graphqlservice PUBLIC
        cppgraphqlgen::graphqlservice
        cppgraphqlgen::graphqljson
        app_base
        )
add_dependencies(taraxa_graphqlservice regenerate_schema_files)

# Pure taraxa graphql service withou internal methods implementations...
#add_library(taraxa_graphqlservice STATIC gen/TaraxaSchema.cpp TaraxaSchemaImpl.cpp)
#target_link_libraries(taraxa_graphqlservice PUBLIC
#    cppgraphqlgen::graphqlservice
#    cppgraphqlgen::graphqljson
#)
#
#add_dependencies(taraxa_graphqlservice regenerate_schema_files)