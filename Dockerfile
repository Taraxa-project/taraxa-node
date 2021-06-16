# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:20.04 as builder

# deps versions
ARG GO_VERSION=1.13.7
ARG CMAKE_VERSION=3.16.3-1ubuntu1
ARG GFLAGS_VERSION=2.2.2-1build1

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
    ./llvm.sh 10 && \
    rm -f llvm.sh

# Install standard tools
RUN apt-get update \
    && apt-get install -y \
    libtool \
    autoconf \
    binutils \
    cmake=$CMAKE_VERSION \
    ccache \
    libgflags-dev=$GFLAGS_VERSION \
    && rm -rf /var/lib/apt/lists/*

ENV CXX="clang++-10"
ENV CC="clang-10"

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

RUN conan remote add -f bincrafters "https://api.bintray.com/conan/bincrafters/public-conan" && \
    conan profile new clang --detect && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=10 clang && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang && \
    conan profile update env.CC=clang-10 clang && \
    conan profile update env.CXX=clang++-10 clang && \
    conan install -if $BUILD_OUTPUT_DIR --build missing -s build_type=Debug -pr=clang .

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
