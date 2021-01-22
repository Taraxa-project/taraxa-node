# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR=cmake-docker-build-release

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:20.04 as builder

# for clang-format and clang-tidy
ARG CLANG_VERSION=1:10.0-50~exp1

# deps versions
ARG GO_VERSION=1.13.7
ARG ROCKSDB_VERSION=5.18.3
ARG BOOST_VERSION=1.71.0.0ubuntu2
ARG CMAKE_VERSION=3.16.3-1ubuntu1
ARG GCC_VERSION=4:9.3.0-1ubuntu2
ARG CPPCHECK_VERSION=1.90-4build1
ARG GFLAGS_VERSION=2.2.2-1build1
ARG SNAPPY_VERSION=1.1.8-1build1
ARG ZLIB1G_VERSION=1:1.2.11.dfsg-2ubuntu1.2
ARG BZ2_VERSION=1.0.8-2
ARG LZ4_VERSION=1.9.2-2
ARG ZSTD_VERSION=1.4.4+dfsg-3
ARG SSL_VERSION=1.1.1f-1ubuntu2.1
ARG JSONCPP_VERSION=1.7.4-3.1ubuntu2
ARG JSONRPCCPP_VERSION=0.7.0-1build3
ARG SCRYPT_VERSION=1.21-3
ARG MPFR_VERSION=4.0.2-1
ARG GMP3_VERSION=2:6.2.0+dfsg-4

# Install standard packages
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt-get install -y \
        tar \
        curl \
        libtool \
        autoconf \
        binutils \
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
# TODO: remove shared rocksdb lib -> tmp hack to make submodules build
RUN curl -SL https://github.com/facebook/rocksdb/archive/v$ROCKSDB_VERSION.tar.gz \
    | tar -xzC /tmp \
    && cd /tmp/rocksdb-${ROCKSDB_VERSION} \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' PORTABLE=1 make -j $(nproc) install-static \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' PORTABLE=1 make -j $(nproc) install-shared \
    && rm -rf $(pwd)
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

# Install go
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
        | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH


###################################################################
# Build stage - use builder image for actual build of taraxa node #
###################################################################
FROM builder as build

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR

# Build taraxa-node project
WORKDIR /opt/taraxa/
COPY . .
RUN mkdir $BUILD_OUTPUT_DIR \
    && cd $BUILD_OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=Release \
             -DTARAXA_STATIC_BUILD=ON \
             -DTARAXAD_INSTALL_DIR=./bin_install \
             -DTARAXAD_CONF_INSTALL_DIR=./bin_install \
             ../ \
    && make -j$(nproc) all \
    && make install \
    && strip bin_install/taraxad \
    && cd tests/ \
    && ctest --output-on-failure


###############################################################################
# Taraxa image containing taraxad binary with statically linked deps + config #
###############################################################################
FROM ubuntu:20.04

ARG BUILD_OUTPUT_DIR
WORKDIR /opt/taraxa/taraxa-node

# Keep the old struct for now
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin_install/taraxad ./main
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin_install/taraxad.conf ./default_config/conf_taraxa1.json

ENTRYPOINT ["./main"]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
