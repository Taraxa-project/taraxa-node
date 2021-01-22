#!/bin/bash

source $(dirname "$0")/common.sh

if [ ! -e boost ]; then
  BOOST_V_MAJOR=1
  BOOST_V_MINOR=71
  BOOST_V_PATCH=0
  part_1=$BOOST_V_MAJOR.$BOOST_V_MINOR.$BOOST_V_PATCH
  part_2=boost_${BOOST_V_MAJOR}_${BOOST_V_MINOR}_${BOOST_V_PATCH}
  echo "Downloading boost $part_1..."
  wget -qO- \
    "https://dl.bintray.com/boostorg/release/$part_1/source/$part_2.tar.bz2" |
    tar --bzip2 -xf -
  mv "$part_2" boost
fi
cd boost
./bootstrap.sh --prefix=${dst}
./b2 --clean
./b2 --prefix=${dst} \
  --with-thread --with-system --with-log \
  --with-filesystem --with-program_options \
  -j${cpu_cnt} link=shared threading=multi \
  cxxflags="-fvisibility=hidden -fvisibility-inlines-hidden" \
  install
