#!/bin/bash

source $(dirname "$0")/common.sh

if [ ! -e llvm ]; then
  echo "downloading LLVM..."
  wget -qO- "https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-apple-darwin.tar.xz" |
    tar -xf -
  mv "clang+llvm-10.0.0-x86_64-apple-darwin" llvm
fi
cd llvm
cp bin/clang-format ${dst}/bin/
