# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

#############################################
# builder image - contains all dependencies #
#############################################
FROM amd64/ubuntu:20.04 as builder

# deps versions
ARG GO_VERSION=1.13.7
ARG CMAKE_VERSION=3.16.3-1ubuntu1
ARG GFLAGS_VERSION=2.2.2-1build1
ARG LLVM_VERSION=12

# Install standard packages
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt-get install -y \
    tar \
    git \
    curl \
    wget \
    python3-pip \
    lsb-release \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*


# Install LLVM
RUN curl -SL -o llvm.sh https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh $LLVM_VERSION && \
    rm -f llvm.sh

# Install standard tools
RUN apt-get update \
    && apt-get install -y \
    clang-format-$LLVM_VERSION \
    clang-tidy-$LLVM_VERSION \
    libtool \
    autoconf \
    binutils \
    cmake=$CMAKE_VERSION \
    ccache \
    libgflags-dev=$GFLAGS_VERSION \
    && rm -rf /var/lib/apt/lists/*

ENV CXX="clang++-${LLVM_VERSION}"
ENV CC="clang-${LLVM_VERSION}"

# Install conan
RUN pip3 install conan

# Install go
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
    | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
ENV CONAN_REVISIONS_ENABLED=1

# Install conan deps
WORKDIR /opt/taraxa/
COPY conanfile.py .

RUN conan remote add -f bincrafters "https://bincrafters.jfrog.io/artifactory/api/conan/public-conan" && \
    conan profile new clang --detect && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=$LLVM_VERSION clang && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang && \
    conan profile update env.CC=clang-$LLVM_VERSION clang && \
    conan profile update env.CXX=clang++-$LLVM_VERSION clang && \
    conan install --build missing -s build_type=Debug -pr=clang .

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
    && cmake -DCMAKE_BUILD_TYPE=Debug \
    -DTARAXA_ENABLE_LTO=OFF \
    -DTARAXA_STATIC_BUILD=OFF \
    ../ \
    && make -j$(nproc) all \
    # Copy CMake generated Testfile to be able to trigger ctest from bin directory
    && cp tests/CTestTestfile.cmake bin/ \
    # keep only required shared libraries and final binaries
    #&& find . -maxdepth 1 ! -name "lib" ! -name "bin" -exec rm -rfv {} \;


###############################################################################
##### Taraxa image containing taraxad binary + dynamic libraries + config #####
###############################################################################
FROM ubuntu:20.04

ARG BUILD_OUTPUT_DIR
WORKDIR /root/.taraxa

# Copy required binaries
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin/taraxad /usr/local/bin/taraxad
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/lib/*.so /usr/local/lib/

# Set LD_LIBRARY_PATH so taraxad binary finds shared libs
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

COPY docker-entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
