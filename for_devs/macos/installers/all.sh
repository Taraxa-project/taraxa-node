#!/bin/bash

cd $(dirname "$0")

./bzip2.sh
./openssl.sh
echo ./llvm.sh ./cppcheck.sh ./boost.sh |
  tr " " "\n" |
  parallel --will-cite -j999
