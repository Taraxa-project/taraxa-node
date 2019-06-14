add_library(cryptopp STATIC IMPORTED)
set(CRYPTOPP_BUILD_CMD $(MAKE) -C submodules/cryptopp)
if (NOT APPLE)
    set(CRYPTOPP_BUILD_CMD
            $(MAKE) -C submodules/cryptopp CXXFLAGS="-DNDEBUG -g2 -O3 -fPIC -DCRYPTOPP_DISABLE_ASM -pthread -pipe -c")
endif ()
add_custom_target(
        cryptopp_build
        COMMAND ${CRYPTOPP_BUILD_CMD}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
set_target_properties(cryptopp PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/submodules/cryptopp/libcryptopp.a)
add_dependencies(cryptopp cryptopp_build)