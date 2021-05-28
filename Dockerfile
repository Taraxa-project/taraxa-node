# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:20.04 as builder

# deps versions
ARG GO_VERSION=1.13.7
ARG CMAKE_VERSION=3.16.3-1ubuntu1
ARG GCC_VERSION=4:9.3.0-1ubuntu2
ARG GFLAGS_VERSION=2.2.2-1build1
ARG CLANG_FORMAT_VERSION=clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04

# Install standard packages
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt-get install -y \
        tar \
        git \
        xz-utils \
        curl \
        libtool \
        autoconf \
        binutils \
        \
        ccache \
        cmake=$CMAKE_VERSION \
        gcc=$GCC_VERSION \
        g++=$GCC_VERSION \
        libgflags-dev=$GFLAGS_VERSION \
        \
        python3-pip \
    && rm -rf /var/lib/apt/lists/*


# install clang-format and clang-tidy
RUN curl -SLs https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/$CLANG_FORMAT_VERSION.tar.xz \
        | tar -xJ && \
    mv $CLANG_FORMAT_VERSION/bin/clang-format /usr/bin/clang-format && \
    mv $CLANG_FORMAT_VERSION/bin/clang-tidy /usr/bin/clang-tidy && \
    rm -rf $CLANG_FORMAT_VERSION && \
    rm -f $CLANG_FORMAT_VERSION.tar.xz

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
RUN conan remote add -f bincrafters "https://api.bintray.com/conan/bincrafters/public-conan"
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
