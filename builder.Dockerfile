#############################################
# builder image - contains all dependencies #
#############################################
FROM ubuntu:22.04

# deps versions
ARG LLVM_VERSION=14

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
RUN \
if [ `arch` != "aarch64" ]; \
then  \
    add-apt-repository ppa:ethereum/ethereum \
    && apt-get update \
    && apt install solc; \
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
RUN pip3 install conan==1.59.0

ENV CONAN_REVISIONS_ENABLED=1

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


