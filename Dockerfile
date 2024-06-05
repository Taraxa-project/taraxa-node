ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:24.04 as builder

# deps versions
ARG LLVM_VERSION=18

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install standard packages
RUN apt-get update && DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
    tzdata \
    && apt-get install -y \
    tar \
    git \
    curl \
    wget \
    python3-pip \
    lsb-release \
    libgmp-dev \
    libmpfr-dev \
    libmicrohttpd-dev \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# install solc for py_test if arch is not arm64 because it is not availiable

# Install solc 0.8.24 as we do not support 0.8.25 yet
RUN \
if [ `arch` != "aarch64" ]; \
then  \
curl -L -o solc-0.8.24 https://github.com/ethereum/solidity/releases/download/v0.8.24/solc-static-linux \
    && chmod +x solc-0.8.24 \
    && mv solc-0.8.24 /usr/bin/solc; \
fi

# install standart tools
RUN add-apt-repository ppa:ethereum/ethereum \
    && apt-get update \
    && apt-get install -y \
    clang-format-$LLVM_VERSION \
    clang-tidy-$LLVM_VERSION \
    llvm-$LLVM_VERSION \
    golang-go \
    ca-certificates \
    libtool \
    autoconf \
    binutils \
    cmake \
    ccache \
    # this libs are required for arm build by go part
    libzstd-dev \
    libsnappy-dev \
    # replace this with conan dependency
    rapidjson-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CXX="clang++-${LLVM_VERSION}"
ENV CC="clang-${LLVM_VERSION}"

# HACK remove this when update to conan 2.0
RUN ln -s /usr/bin/clang-${LLVM_VERSION} /usr/bin/clang
RUN ln -s /usr/bin/clang++-${LLVM_VERSION} /usr/bin/clang++

# Install conan
RUN apt-get remove -y python3-distro
RUN pip3 install conan==1.64.1 --break-system-packages
COPY CMakeModules/settings.yml /root/.conan/settings.yml

# Install conan deps
WORKDIR /opt/taraxa/
COPY conanfile.py .

RUN conan profile new clang --detect \
    && conan profile update settings.compiler=clang clang  \
    && conan profile update settings.compiler.version=$LLVM_VERSION clang  \
    && conan profile update settings.compiler.libcxx=libstdc++11 clang \
    && conan profile update settings.build_type=RelWithDebInfo clang \
    && conan profile update env.CC=clang-$LLVM_VERSION clang  \
    && conan profile update env.CXX=clang++-$LLVM_VERSION clang  \
    && conan install --build missing -pr=clang .

###################################################################
# Build stage - use builder image for actual build of taraxa node #
###################################################################
FROM builder as build

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR

# Build taraxa-node project
WORKDIR /opt/taraxa/
COPY . .

RUN mkdir $BUILD_OUTPUT_DIR && cd $BUILD_OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DTARAXA_ENABLE_LTO=OFF \
    -DTARAXA_STATIC_BUILD=OFF \
    ../

RUN cd $BUILD_OUTPUT_DIR && make -j$(nproc) all \
    # Copy CMake generated Testfile to be able to trigger ctest from bin directory
    && cp tests/CTestTestfile.cmake bin/;
    # \
    # keep only required shared libraries and final binaries
    # && find . -maxdepth 1 ! -name "lib" ! -name "bin" -exec rm -rfv {} \;

# Set LD_LIBRARY_PATH so taraxad binary finds shared libs
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

###############################################################################
##### Taraxa image containing taraxad binary + dynamic libraries + config #####
###############################################################################
FROM ubuntu:24.04

# Install curl and jq
RUN apt-get update \
    && apt-get install -y curl jq python3 python3-pip python3-virtualenv \
    && rm -rf /var/lib/apt/lists/*

# Install required Python packages
RUN pip3 install click eth-account eth-utils typing-extensions --break-system-packages

ARG BUILD_OUTPUT_DIR
WORKDIR /root/.taraxa

# Copy required binaries
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin/taraxad /usr/local/bin/taraxad
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin/taraxa-bootnode /usr/local/bin/taraxa-bootnode
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/lib/*.so* /usr/local/lib/

# Copy scripts
COPY scripts/taraxa-sign.py /usr/local/bin/taraxa-sign

# Set LD_LIBRARY_PATH so taraxad binary finds shared libs
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

COPY docker-entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
