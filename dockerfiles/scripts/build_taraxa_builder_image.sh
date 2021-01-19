#!/bin/bash

source ./taraxa_variables.conf
source scripts/common.sh

# Load taraxa_builder.dockerfile specific ARG's from taraxa_variables.conf
function LoadTaraxaBuilderDockerArgs {
  ret_val="$(LoadConfigVar GO_VERSION)
    $(LoadConfigVar ROCKSDB_VERSION)
    $(LoadConfigVar GO_VERSION)
    $(LoadConfigVar CMAKE_VERSION)
    $(LoadConfigVar GCC_VERSION)
    $(LoadConfigVar CLANG_VERSION)
    $(LoadConfigVar CPPCHECK_VERSION)
    $(LoadConfigVar BOOST_VERSION)
    $(LoadConfigVar GFLAGS_VERSION)
    $(LoadConfigVar SNAPPY_VERSION)
    $(LoadConfigVar ZLIB1G_VERSION)
    $(LoadConfigVar BZ2_VERSION)
    $(LoadConfigVar LZ4_VERSION)
    $(LoadConfigVar ZSTD_VERSION)
    $(LoadConfigVar SSL_VERSION)
    $(LoadConfigVar JSONCPP_VERSION)
    $(LoadConfigVar JSONRPCCPP_VERSION)
    $(LoadConfigVar SCRYPT_VERSION)
    $(LoadConfigVar MPFR_VERSION)
    $(LoadConfigVar GMP3_VERSION)
    "

  echo "$ret_val"
}

echo $CYAN"Building taraxa_builder image, "$taraxa_builder_image"..."$COLOR_END
DOCKER_BUILDKIT=1 docker build $(LoadTaraxaBuilderDockerArgs) --progress=plain -t $taraxa_builder_image -f taraxa_builder.dockerfile .
echo $CYAN"Building taraxa_builder image, "$taraxa_builder_image" completed."$COLOR_END
