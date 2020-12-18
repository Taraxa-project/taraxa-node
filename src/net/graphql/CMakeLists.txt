cmake_minimum_required(VERSION 3.8.2)

find_package(pegtl 3.0.0 CONFIG REQUIRED)
find_package(cppgraphqlgen 3.3.0 CONFIG REQUIRED)

# unifiedschema
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen)

add_custom_command(
        OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TodaySchema.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TodaySchema.h
        COMMAND cppgraphqlgen::schemagen --schema="${CMAKE_CURRENT_SOURCE_DIR}/schema/schema.today.graphql" --prefix="Today" --namespace="today"
        DEPENDS schema/schema.today.graphql
        WORKING_DIRECTORY gen
        COMMENT "Generating mock TodaySchema files")

file(GLOB OLD_GEN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/gen/*)

add_custom_command(
        OUTPUT updated_samples
        COMMAND ${CMAKE_COMMAND} -E remove -f ${OLD_GEN_FILES}
        COMMAND ${CMAKE_COMMAND} -E copy_directory gen/ ${CMAKE_CURRENT_SOURCE_DIR}/gen/
        COMMAND ${CMAKE_COMMAND} -E touch updated_samples
        DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TodaySchema.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/gen/TodaySchema.h
        COMMENT "Updating sample files")

add_custom_target(update_samples ALL DEPENDS updated_samples)

add_library(taraxa_graphqlservice STATIC gen/TodaySchema.cpp TodayMock.cpp)
target_link_libraries(taraxa_graphqlservice PUBLIC cppgraphqlgen::graphqlservice cppgraphqlgen::graphqljson)
add_dependencies(taraxa_graphqlservice update_samples)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE taraxa_graphqlservice)
add_dependencies(benchmark update_samples)
