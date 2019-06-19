include_guard()
include(${PROJECT_SOURCE_DIR}/cmake/external_lib.cmake)

external_lib(secp256k1 STATIC ${PROJECT_SOURCE_DIR}/submodules/secp256k1/.libs/libsecp256k1.a
        COMMAND
        test ! -e Makefile
        &&
        ./autogen.sh &&
        ./configure --disable-shared --disable-tests --disable-coverage --disable-openssl-tests
        --disable-exhaustive-tests --disable-jni --with-bignum=no
        --with-field=64bit --with-scalar=64bit
        --with-asm=no --enable-module-ecdh --enable-module-recovery --enable-experimental
        "\;"
        make
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/submodules/secp256k1)