# Taraxa remote docker environment - it is intended to be used for taraxa-node building
#
# Build and run:
#
#   DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa/taraxa_builder:1.0 -f taraxa_builder.dockerfile .
#   docker run -ti --name taraxa_builder taraxa/taraxa_builder:1.0 /bin/bash
#
# stop:
#   docker stop taraxa_builder
#
# users:
#   root

FROM ubuntu:20.04

# Do not set these variables directly here, specify it's values in taraxa_variables.conf file
ARG ROCKSDB_VERSION
ARG GO_VERSION
ARG CMAKE_VERSION
ARG GCC_VERSION
ARG CLANG_VERSION
ARG CPPCHECK_VERSION
ARG BOOST_VERSION
ARG GFLAGS_VERSION
ARG SNAPPY_VERSION
ARG ZLIB1G_VERSION
ARG BZ2_VERSION
ARG LZ4_VERSION
ARG ZSTD_VERSION
ARG SSL_VERSION
ARG JSONCPP_VERSION
ARG JSONRPCCPP_VERSION
ARG SCRYPT_VERSION
ARG MPFR_VERSION
ARG GMP3_VERSION

# Install standard packages
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt-get install -y \
        tar \
        curl \
        libtool \
        autoconf \
        \
        ccache \
        cmake=$CMAKE_VERSION \
        gcc=$GCC_VERSION \
        g++=$GCC_VERSION \
        clang-format=$CLANG_VERSION \
        clang-tidy=$CLANG_VERSION \
        cppcheck=$CPPCHECK_VERSION \
        \
        libboost-program-options-dev=$BOOST_VERSION \
        libboost-system-dev=$BOOST_VERSION \
        libboost-filesystem-dev=$BOOST_VERSION \
        libboost-thread-dev=$BOOST_VERSION \
        libboost-log-dev=$BOOST_VERSION \
        \
        libgflags-dev=$GFLAGS_VERSION \
        libsnappy-dev=$SNAPPY_VERSION \
        zlib1g-dev=$ZLIB1G_VERSION \
        libbz2-dev=$BZ2_VERSION \
        liblz4-dev=$LZ4_VERSION \
        libzstd-dev=$ZSTD_VERSION \
        \
        libssl-dev=$SSL_VERSION \
        libjsoncpp-dev=$JSONCPP_VERSION \
        libjsonrpccpp-dev=$JSONRPCCPP_VERSION \
        libscrypt-dev=$SCRYPT_VERSION \
        libmpfr-dev=$MPFR_VERSION \
        libgmp3-dev=$GMP3_VERSION \
    && rm -rf /var/lib/apt/lists/*

# Install rocksdb
RUN curl -SL https://github.com/facebook/rocksdb/archive/v$ROCKSDB_VERSION.tar.gz \
    | tar -xzC /tmp \
    && cd /tmp/rocksdb-${ROCKSDB_VERSION} \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' make -j $(nproc) install-static \
    && rm -rf $(pwd)
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

# Install go
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
        | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
