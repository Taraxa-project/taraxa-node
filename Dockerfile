# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:20.04 as builder

# for clang-format and clang-tidy
ARG CLANG_VERSION=1:10.0-50~exp1

# deps versions
ARG GO_VERSION=1.13.7
ARG CMAKE_VERSION=3.16.3-1ubuntu1
ARG GCC_VERSION=4:9.3.0-1ubuntu2
ARG GFLAGS_VERSION=2.2.2-1build1
ARG JSONCPP_VERSION=1.7.4-3.1ubuntu2
ARG JSONRPCCPP_VERSION=0.7.0-1build3
ARG SCRYPT_VERSION=1.21-3

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
        libgflags-dev=$GFLAGS_VERSION \
        libjsoncpp-dev=$JSONCPP_VERSION \
        libjsonrpccpp-dev=$JSONRPCCPP_VERSION \
        libscrypt-dev=$SCRYPT_VERSION \
        \
        python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install conan
RUN pip3 install conan

# Install go
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
        | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR

# Install conan deps
WORKDIR /opt/taraxa/
COPY conanfile.py .
RUN conan install -if $BUILD_OUTPUT_DIR --build missing -s build_type=Debug .


###################################################################
# Build stage - use builder image for actual build of taraxa node #
###################################################################
FROM builder as build

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR

# Build taraxa-node project
WORKDIR /opt/taraxa/
COPY . .
RUN cd $BUILD_OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=Debug \
             -DTARAXA_ENABLE_LTO=ON \
             -DTARAXA_STATIC_BUILD=ON \
             -DTARAXAD_INSTALL_DIR=./bin_install \
             -DTARAXAD_CONF_INSTALL_DIR=./bin_install \
             ../ \
    && make -j$(nproc) all \
    && make install

###############################################################################
# Taraxa Cli #
###############################################################################
FROM ubuntu:20.04 as cli

WORKDIR /opt/taraxa

RUN apt-get update \
    && apt-get install -y python3-pip

COPY cli/requirements.txt cli/requirements.txt
RUN pip3 install --no-cache-dir -r cli/requirements.txt

COPY cli/taraxa cli/taraxa


###############################################################################
# Taraxa image containing taraxad binary with statically linked deps + config #
###############################################################################
FROM cli

ARG BUILD_OUTPUT_DIR
WORKDIR /opt/taraxa

# Keep the old struct for now
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin_install/taraxad /usr/local/bin/taraxad
COPY config config
COPY docker-entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
