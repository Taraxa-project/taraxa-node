#!/bin/bash

source $(dirname "$0")/common.sh

if [ ! -e cppcheck ]; then
  ver=1.90
  echo "Downloading cppcheck..."
  wget -qO- "https://github.com/danmar/cppcheck/archive/${ver}.tar.gz" |
    tar --bzip2 -xf -
  mv cppcheck-${ver} cppcheck
fi
cd cppcheck
cmake -DCMAKE_INSTALL_PREFIX=${dst} . -B build
cmake --build build -j ${cpu_cnt}
cmake --build build --target install -j ${cpu_cnt}
