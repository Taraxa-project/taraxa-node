ARG BUILD_OUTPUT_DIR=cmake-docker-build
ARG WORKDIR=/opt/taraxa
ARG BUILD_TYPE=RelWithDebInfo

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:24.04@sha256:e3f92abc0967a6c19d0dfa2d55838833e947b9d74edbcb0113e48535ad4be12a AS builder

# deps versions
ENV LLVM_VERSION=18

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install standard packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    tzdata \
    && apt-get install -y \
    tar \
    git \
    curl \
    wget \
    python3-pip \
    lsb-release \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# install solc for py_test if arch is not arm64 because it is not available

# Install solc 0.8.24 as we do not support 0.8.25 yet
RUN \
if [ `arch` != "aarch64" ]; \
then  \
curl -L -o solc-0.8.25 https://github.com/ethereum/solidity/releases/download/v0.8.25/solc-static-linux \
    && chmod +x solc-0.8.25 \
    && mv solc-0.8.25 /usr/bin/solc; \
fi

# install standard tools
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
    libmicrohttpd-dev \
    libgoogle-perftools-dev \
    # these libs are required for arm build by go part
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
RUN pip3 install conan --break-system-packages

WORKDIR $WORKDIR
COPY scripts scripts
COPY conanfile.py .

ARG BUILD_OUTPUT_DIR

# Create directory before using it
RUN mkdir -p $BUILD_OUTPUT_DIR
RUN ./scripts/config.sh
ARG BUILD_TYPE
RUN conan install . -s "build_type=Release" -s "&:build_type=$BUILD_TYPE" --profile:host=clang --profile:build=clang --build=missing --output-folder=$BUILD_OUTPUT_DIR

###################################################################
# Build stage - use builder image for actual build of taraxa node #
###################################################################
FROM builder AS build

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR
ARG BUILD_TYPE

WORKDIR $WORKDIR
COPY . .

# Remove the duplicate mkdir and combine commands
RUN cd $BUILD_OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DTARAXA_ENABLE_LTO=OFF \
    -DTARAXA_STATIC_BUILD=OFF \
    -DTARAXA_GPERF=ON \
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
FROM ubuntu:24.04@sha256:e3f92abc0967a6c19d0dfa2d55838833e947b9d74edbcb0113e48535ad4be12a

# Install curl and jq
RUN apt-get update \
    && apt-get install -y \
        curl \
        jq \
        python3 \
        python3-pip \
        python3-virtualenv \
        libgoogle-perftools4t64 \
    && rm -rf /var/lib/apt/lists/*

# Install required Python packages
RUN pip3 install click eth-account eth-utils typing-extensions --break-system-packages

ARG BUILD_OUTPUT_DIR
WORKDIR /root/.taraxa

# Copy required binaries
COPY --from=build $WORKDIR/$BUILD_OUTPUT_DIR/bin/taraxad /usr/local/bin/taraxad
COPY --from=build $WORKDIR/$BUILD_OUTPUT_DIR/bin/taraxa-bootnode /usr/local/bin/taraxa-bootnode
COPY --from=build $WORKDIR/$BUILD_OUTPUT_DIR/lib/*.so* /usr/local/lib/

# Copy scripts
COPY scripts/taraxa-sign.py /usr/local/bin/taraxa-sign

# Set LD_LIBRARY_PATH so taraxad binary finds shared libs
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

COPY docker-entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
