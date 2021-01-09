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
#   root, passwd: root

FROM ubuntu:20.04

# Set password for root -> root
RUN echo "root:root" | chpasswd

# Install standard packages
RUN apt-get update

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata
RUN apt-get install -y \
      nano \
      tar \
      git \
      wget \
      cmake ccache gcc g++ clang clang-format clang-tidy gdb \
      libboost-program-options-dev libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-log-dev \
      libtool \
      autoconf \
      libssl-dev \
      libjsoncpp-dev libjsonrpccpp-dev \
      libscrypt-dev \
      librocksdb-dev \
      libmpfr-dev \
      libgmp3-dev
RUN apt-get clean

# Install go
ARG GO_ARCHIVE=go1.13.7.linux-amd64.tar.gz
RUN wget https://dl.google.com/go/$GO_ARCHIVE
RUN tar -xf $GO_ARCHIVE -C /usr/local
RUN rm -rf $GO_ARCHIVE

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
