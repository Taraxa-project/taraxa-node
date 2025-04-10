./config.sh

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

if [ -z "$BUILD_DIR" ]; then
    echo 'BUILD_DIR is not specified. Defaulting to "./build"'
    export BUILD_DIR=${SCRIPTPATH}/../build
fi

if [ -z "$SOURCE_DIR" ]; then
    echo 'SOURCE_DIR is not specified. Defaulting to "../"'
    export SOURCE_DIR=${SCRIPTPATH}/../
fi

# build and use conan deps in Release mode
conan install ${SOURCE_DIR} -s "build_type=Release" -s "&:build_type=${CMAKE_BUILD_TYPE}" --profile:host=clang --profile:build=clang --build=missing --output-folder=$BUILD_DIR

export CPU_COUNT=$(scripts/cpu_count.sh)

cd $BUILD_DIR
cmake ${SOURCE_DIR}
make -j $CPU_COUNT
