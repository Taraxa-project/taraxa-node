#!/bin/bash

# stop on ctrl+c
trap "exit 1" INT

SCRIPTPATH=$(dirname $(realpath "$0"))
source ${SCRIPTPATH}/config.sh

if [ -z "$BUILD_DIR" ]; then
    export BUILD_DIR=$(realpath ${SCRIPTPATH}/../build)
    echo 'BUILD_DIR is not specified. Defaulting to "'${BUILD_DIR}'"'
fi

if [ -z "$SOURCE_DIR" ]; then
    export SOURCE_DIR=$(realpath ${SCRIPTPATH}/../)
    echo 'SOURCE_DIR is not specified. Defaulting to "'${SOURCE_DIR}'"'
fi

# build and use conan deps in Release mode
conan install ${SOURCE_DIR} -s "build_type=Release" -s "&:build_type=${CMAKE_BUILD_TYPE}" --profile:host=clang --profile:build=clang --build=missing --output-folder=${BUILD_DIR}

export CPU_COUNT=$(${SCRIPTPATH}/cpu_count.sh)
echo "Building taraxa-node with ${CPU_COUNT} threads"
cd $BUILD_DIR
cmake ${SOURCE_DIR}
make -j $CPU_COUNT

echo "Build completed successfully in ${BUILD_DIR}"