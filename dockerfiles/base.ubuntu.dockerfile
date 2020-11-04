FROM ubuntu:20.04 as builder
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm

RUN \
# remember build-only deps
echo \
    zlib1g-dev libbz2-dev libzstd-dev gcc-8 g++-8 clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential \
    xz-utils curl libcurl4-openssl-dev pkg-config git python2-dev \
    libxml2-dev libxslt-dev autoconf libtool busybox liblz4-dev \
    python3-pip python3-dev clang-format cppcheck binutils \
>> /apt_deps_build.txt && \
# remember runtime-only deps to install only them in the final layer
echo \
    libjsoncpp-dev libjsonrpccpp-dev libicu-dev \
    libmpfr-dev libgmp3-dev libscrypt-dev libssl-dev \
>> /apt_deps_runtime.txt && \
apt-get update && \
apt-get install -y $(cat /apt_deps_build.txt) $(cat /apt_deps_runtime.txt) && \
apt-get autoclean && \
ln -s /usr/include/jsoncpp/json /usr/include/json && \
update-alternatives --install \
    /usr/bin/gcc gcc /usr/bin/gcc-8 800 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-8 && \
pip3 install --upgrade pip cmake==3.17.2

# description is inside the script
COPY dockerfiles/symlink_index.sh /
RUN chmod 777 /symlink_index.sh
RUN cd /usr/local/lib && /symlink_index.sh restore build

# Add multi-stage build for docker layer caching
FROM builder as boost-layer
ENV BOOST_V_MAJOR=1
ENV BOOST_V_MINOR=71
ENV BOOST_V_PATCH=0
RUN \
cd /tmp && \
part_1=$BOOST_V_MAJOR.$BOOST_V_MINOR.$BOOST_V_PATCH && \
part_2=boost_${BOOST_V_MAJOR}_${BOOST_V_MINOR}_${BOOST_V_PATCH} && \
echo "Downloading boost $part_1..." && \
wget -qO- \
    "https://dl.bintray.com/boostorg/release/$part_1/source/$part_2.tar.bz2" \
    | tar --bzip2 -xf - && \
cd "$part_2" && \
./bootstrap.sh && \
./b2 \
    --with-thread --with-system --with-log \
    --with-filesystem --with-program_options \
    -j$(nproc) link=shared threading=multi \
    install && \
rm -rf $(pwd)
RUN cd /usr/local/lib && /symlink_index.sh restore build

FROM boost-layer as rocksdb-layer
ARG rocksdb_version=5.18.3
ENV ROCKSDB_VERSION="$rocksdb_version"
RUN \
cd /tmp && \
echo "Downloading rocksdb $rocksdb_version..." && \
wget -qO- \
    https://github.com/facebook/rocksdb/archive/v$rocksdb_version.zip \
    | busybox unzip -q - && \
cd rocksdb-${rocksdb_version} && \
chmod -R 777 . && \
make -j $(nproc) PORTABLE=1 install-shared && \
rm -rf $(pwd)
RUN cd /usr/local/lib && /symlink_index.sh restore build

FROM rocksdb-layer as go-layer
ARG go_version=1.13.7
RUN \
echo "Downloading golang $go_version..." && \
wget -qO- https://dl.google.com/go/go$go_version.linux-amd64.tar.gz \
    | tar xvz -C /usr/local
RUN cd /usr/local/lib && /symlink_index.sh restore build

ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH

FROM go-layer as release
# some submodules support debug mode
ARG DEBUG=1
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib
WORKDIR ${APP_PATH}
COPY . .
RUN \
# install submodules
make -f Makefile_submodules.mk -j $(nproc) \
    DEPS_INSTALL_PREFIX=/usr/local \
    DEBUG=$DEBUG \
    submodules && \
# keep only essential files in the submodule source directories
find submodules/ ! -type d \
    ! -path "*/ok" -and \
    ! -path "submodules/taraxa-evm/taraxa/data/eth_mainnet_blocks_0_300000.rlp" \
    -delete && \
find submodules/ -type d -empty -delete && \
rm -rf /tmp/*
RUN cd /usr/local/lib && /symlink_index.sh restore build
