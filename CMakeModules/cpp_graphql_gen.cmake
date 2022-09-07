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
set(GRAPHQL_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/network/graphql/gen)
list(APPEND GRAPHQL_GENERATED_SOURCES
    ${GRAPHQL_GEN_DIR}/AccountObject.cpp
    ${GRAPHQL_GEN_DIR}/BlockObject.cpp
    ${GRAPHQL_GEN_DIR}/CallResultObject.cpp
    ${GRAPHQL_GEN_DIR}/CurrentStateObject.cpp
    ${GRAPHQL_GEN_DIR}/DagBlockObject.cpp
    ${GRAPHQL_GEN_DIR}/LogObject.cpp
    ${GRAPHQL_GEN_DIR}/MutationObject.cpp
    ${GRAPHQL_GEN_DIR}/PendingObject.cpp
    ${GRAPHQL_GEN_DIR}/QueryObject.cpp
    ${GRAPHQL_GEN_DIR}/SubscriptionObject.cpp
    ${GRAPHQL_GEN_DIR}/SyncStateObject.cpp
    ${GRAPHQL_GEN_DIR}/TaraxaSchema.cpp
    ${GRAPHQL_GEN_DIR}/TransactionObject.cpp
)

add_custom_command(OUTPUT ${GRAPHQL_GENERATED_SOURCES}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${GRAPHQL_GEN_DIR}/*.cpp
        COMMAND ${CMAKE_COMMAND} -E remove -f ${GRAPHQL_GEN_DIR}/*.h
        COMMAND cppgraphqlgen::schemagen --schema="${CMAKE_CURRENT_SOURCE_DIR}/network/graphql/schema/schema.taraxa.graphql" --prefix="Taraxa" --namespace="taraxa"
        WORKING_DIRECTORY ${GRAPHQL_GEN_DIR}
        COMMENT "Regenerating TaraxaSchema files")
