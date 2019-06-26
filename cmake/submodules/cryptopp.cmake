include_guard()
include(${PROJECT_SOURCE_DIR}/cmake/external_lib.cmake)

set(CRYPTOPP_BUILD_CMD $(MAKE) -C ${PROJECT_SOURCE_DIR}/submodules/cryptopp)
if (NOT APPLE)
    set(CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_ASM)
    set(CRYPTOPP_BUILD_CMD
            ${CRYPTOPP_BUILD_CMD} CXXFLAGS='-DNDEBUG -g2 -O3 -fPIC -D${CRYPTOPP_COMPILE_DEFINITIONS} -pthread -pipe -c')
endif ()
external_lib(cryptopp STATIC ${PROJECT_SOURCE_DIR}/submodules/cryptopp/libcryptopp.a
        COMMAND ${CRYPTOPP_BUILD_CMD}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
target_compile_definitions(cryptopp INTERFACE ${CRYPTOPP_COMPILE_DEFINITIONS})