add_library(secp256k1 STATIC IMPORTED)
add_custom_target(
        secp256k1_build
        COMMAND ./autogen.sh
        COMMAND ./configure --disable-shared --disable-tests --disable-coverage --disable-openssl-tests
        --disable-exhaustive-tests --disable-jni --with-bignum=no --with-field=64bit --with-scalar=64bit
        --with-asm=no --enable-module-ecdh --enable-module-recovery --enable-experimental
        COMMAND make
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/submodules/secp256k1
)
set_target_properties(secp256k1 PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/submodules/secp256k1/.libs/libsecp256k1.a)
add_dependencies(secp256k1 secp256k1_build)