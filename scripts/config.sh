#!/bin/bash

SCRIPTPATH=$(dirname $(realpath "$0"))

source $SCRIPTPATH/sed_cmd.sh

if [ -z "$CMAKE_BUILD_TYPE" ]; then
    echo 'CMAKE_BUILD_TYPE is not specified. Defaulting to "Release"'
    export CMAKE_BUILD_TYPE=Release
fi

conan profile show --profile:host=clang --profile:build=clang > /dev/null 2>&1
# go inside if on error
if [ $? -ne 0 ]; then
    conan profile detect --name clang
fi

PROFILE_PATH=$(conan profile path clang)
sed "${SED_CMD}" "s|cppstd=.*|cppstd=20|" "$PROFILE_PATH"
sed "${SED_CMD}" "s|compiler=.*|compiler=clang|" "$PROFILE_PATH"
sed "${SED_CMD}" "s|compiler.version=.*|compiler.version=18|" "$PROFILE_PATH"
sed "${SED_CMD}" "s/build_type=.*/build_type=${CMAKE_BUILD_TYPE}/" "$PROFILE_PATH"

if [ -z "$LLVM_VERSION" ]; then
    echo "LLVM_VERSION is not specified. Defaulting to 18"
    export LLVM_VERSION=18
fi

if [ "$(uname)" == "Darwin" ]; then
    export CC=/opt/homebrew/opt/llvm@${LLVM_VERSION}/bin/clang
    export CXX=/opt/homebrew/opt/llvm@${LLVM_VERSION}/bin/clang++
else
    export CC=clang-${LLVM_VERSION}
    export CXX=clang++-${LLVM_VERSION}
fi
